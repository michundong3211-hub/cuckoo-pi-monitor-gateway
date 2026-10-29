import importlib.util
from pathlib import Path
import shutil
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SOURCE = Path(__file__).resolve().parent / 'fixtures'
PATCH = ROOT / 'script/patch_espframework_gmt154_wifi_scan.py'


class WifiScanPatchTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='gmt154-wifi-scan-')
        self.directory = Path(self.temp.name)
        for name in ('basewebserver.cpp', 'basewebserver.hpp'):
            shutil.copy2(SOURCE / name, self.directory / name)

    def tearDown(self):
        self.temp.cleanup()

    def test_patch_is_idempotent_and_preserves_api_contract(self):
        spec = importlib.util.spec_from_file_location('wifi_patch', PATCH)
        patch = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(patch)
        self.assertEqual(patch.patch_directory(self.directory), 'changed')
        source = (self.directory / 'basewebserver.cpp').read_text()
        header = (self.directory / 'basewebserver.hpp').read_text()
        self.assertIn('xSemaphoreCreateMutexStatic', header)
        self.assertNotIn('xSemaphoreCreateMutex();', header)
        self.assertIn('WiFi.scanNetworks(true, false)', source)
        self.assertIn('WiFi.scanComplete()', source)
        self.assertIn('WiFi.scanDelete()', source)
        self.assertIn('if (noNetwork < 0) esp_wifi_scan_stop();\n    WiFi.scanDelete();', source)
        self.assertNotIn('WiFi.scanNetworks(false, false)', source)
        self.assertIn('obj[PARAM_SUCCESS] = true;', source)
        self.assertIn('request->send(200, "application/json", scanData);', source)
        before = {p.name: p.read_bytes() for p in self.directory.iterdir()}
        self.assertEqual(patch.patch_directory(self.directory), 'already')
        self.assertEqual(before, {p.name: p.read_bytes() for p in self.directory.iterdir()})

    def test_unknown_source_fails_without_partial_edits(self):
        spec = importlib.util.spec_from_file_location('wifi_patch', PATCH)
        patch = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(patch)
        (self.directory / 'basewebserver.hpp').write_text('unknown revision')
        before = {p.name: p.read_bytes() for p in self.directory.iterdir()}
        with self.assertRaises(RuntimeError):
            patch.patch_directory(self.directory)
        self.assertEqual(before, {p.name: p.read_bytes() for p in self.directory.iterdir()})

    def test_changed_blocking_loop_fails_closed(self):
        spec = importlib.util.spec_from_file_location('wifi_patch', PATCH)
        patch = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(patch)
        target = self.directory / 'basewebserver.cpp'
        target.write_text(target.read_text().replace('WiFi.scanNetworks(false, false)', 'WiFi.scanNetworks(false, true)'))
        before = {p.name: p.read_bytes() for p in self.directory.iterdir()}
        with self.assertRaises(RuntimeError):
            patch.patch_directory(self.directory)
        self.assertEqual(before, {p.name: p.read_bytes() for p in self.directory.iterdir()})

    def test_stop_marker_does_not_accept_reverted_blocking_loop(self):
        spec = importlib.util.spec_from_file_location('wifi_patch', PATCH)
        patch = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(patch)
        target = self.directory / 'basewebserver.cpp'
        target.write_text(target.read_text() + '\n// if (noNetwork < 0) esp_wifi_scan_stop();\n')
        before = {p.name: p.read_bytes() for p in self.directory.iterdir()}
        with self.assertRaises(RuntimeError):
            patch.patch_directory(self.directory)
        self.assertEqual(before, {p.name: p.read_bytes() for p in self.directory.iterdir()})

    def test_hook_only_registers_for_approved_gmt154_environments(self):
        spec = importlib.util.spec_from_file_location('wifi_patch', PATCH)
        patch = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(patch)

        class FakeEnvironment:
            def __init__(self, name):
                self.name = name
                self.middleware = None

            def get(self, key):
                return self.name if key == 'PIOENV' else None

            def subst(self, value):
                if value != '$PROJECT_LIBDEPS_DIR':
                    raise AssertionError(value)
                return str(self.temp_path)

            def AddBuildMiddleware(self, middleware):
                self.middleware = middleware

        patch.Import = lambda name: None
        for name in ('gmt154-06', 'gmt154-06-c2b-diag', 'gmt154-06-c2c-diag', 'gateway28', 'gateway35s3wave-tft-sd'):
            fake = FakeEnvironment(name)
            fake.temp_path = self.directory
            patch.env = fake
            patch.register_hook()
            self.assertEqual(fake.middleware is not None, name.startswith('gmt154-06'))

        libdeps = self.directory / 'diag-libdeps'
        diag_directory = libdeps / 'gmt154-06-wifi-scan-diag/espframework/src'
        diag_directory.mkdir(parents=True)
        diag_source = ROOT / '.pio/libdeps/gmt154-06-wifi-scan-diag/espframework/src/basewebserver.cpp'
        target = diag_directory / 'basewebserver.cpp'
        shutil.copy2(diag_source, target)
        canonical = target.read_text(encoding='utf-8')

        def run_diag_hook():
            fake = FakeEnvironment('gmt154-06-wifi-scan-diag')
            fake.temp_path = libdeps
            patch.env = fake
            patch.register_hook()
            self.assertIsNotNone(fake.middleware)
            fake.middleware(fake, object())

        before = {p: p.read_bytes() for p in diag_directory.rglob('*') if p.is_file()}
        run_diag_hook()
        self.assertEqual(before, {p: p.read_bytes() for p in diag_directory.rglob('*') if p.is_file()})
        run_diag_hook()
        self.assertEqual(before, {p: p.read_bytes() for p in diag_directory.rglob('*') if p.is_file()})

        for original, drifted in (
            ('const bool pending = _wifiScanTask;',
             'const bool pending = !_wifiScanTask;'),
            ('if (pending) {', 'if (!pending) {'),
            ('if (noNetwork == WIFI_SCAN_RUNNING) return;',
             'if (noNetwork != WIFI_SCAN_RUNNING) return;'),
        ):
            with self.subTest(control_flow=original):
                self.assertEqual(canonical.count(original), 1)
                target.write_text(canonical.replace(original, drifted), encoding='utf-8')
                mutated = {p: p.read_bytes() for p in diag_directory.rglob('*') if p.is_file()}
                with self.assertRaises(RuntimeError):
                    run_diag_hook()
                self.assertEqual(mutated, {p: p.read_bytes() for p in diag_directory.rglob('*') if p.is_file()})


if __name__ == '__main__':
    unittest.main()
