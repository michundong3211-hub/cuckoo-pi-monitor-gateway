"""Exercise the diagnostic dependency patch on an isolated real source copy."""
import importlib.util
from pathlib import Path
import re
import runpy
import shutil
import subprocess
import tempfile
import textwrap
import unittest

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / '.pio/libdeps/gmt154-06/espframework/src'
PATCH = ROOT / 'script/patch_espframework_gmt154_wifi_scan_diag.py'


class WifiScanDiagnosticPatchTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='gmt154-wifi-diag-')
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)
        for name in ('basewebserver.cpp', 'wificonnection.cpp', 'improvWiFi/ImprovWiFi.cpp'):
            target = self.work / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(SOURCE / name, target)
        spec = importlib.util.spec_from_file_location('scan_diag_patch', PATCH)
        self.patch = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.patch)

    def test_patch_is_idempotent_and_keeps_scan_operations(self):
        names = ('basewebserver.cpp', 'wificonnection.cpp', 'improvWiFi/ImprovWiFi.cpp')
        before = {name: (self.work / name).read_text(encoding='utf-8') for name in names}
        self.assertEqual(self.patch.patch_directory(self.work), 'changed')
        web = (self.work / 'basewebserver.cpp').read_text(encoding='utf-8')
        operation = re.compile(
            r'WiFi\.scanNetworks\([^;]*?\)|WiFi\.scanComplete\(\)|'
            r'esp_wifi_scan_stop\(\)|WiFi\.scanDelete\(\)'
        )
        for name in names:
            after = (self.work / name).read_text(encoding='utf-8')
            self.assertEqual(operation.findall(after), operation.findall(before[name]), name)
        self.assertIn('#else\n    Log.notice(F("WEB : Scan complete %d, %s." CR)', web)
        wifi = (self.work / 'wificonnection.cpp').read_text(encoding='utf-8')
        improv = (self.work / 'improvWiFi/ImprovWiFi.cpp').read_text(encoding='utf-8')
        header = (self.work / 'gmt154_wifi_scan_diag.hpp').read_text(encoding='utf-8')
        for source, label in ((web, 'web'), (wifi, 'strongest'), (improv, 'improv')):
            self.assertIn('WSCAN: ' + label, source)
        for safe_log in (
            'Log.notice(F("WIFI: Creating access point." CR));',
            'Log.notice(F("WIFI: Connected to wifi ip=%s." CR), getIPAddress().c_str());',
            'Log.warning(F("WIFI: Failed to connect to configured network 1." CR));',
            'Log.notice(F("WIFI: Connected to configured network 2." CR));',
        ):
            self.assertIn(safe_log, wifi)
        for secret_log in (
            '#else\n  Log.notice(F("WIFI: Creating AP with %s,%s." CR)',
            '#else\n  Log.notice(F("WIFI: Connected to wifi %s ip=%s." CR)',
            '#else\n      Log.warning(F("WIFI: Failed to connect to first SSID %s." CR)',
            '#else\n          Log.notice(F("WIFI: Connected to second SSID %s." CR)',
        ):
            self.assertIn(secret_log, wifi)
        self.assertIn('scan_id', header)
        for source in (web, wifi, improv, header):
            for line in source.splitlines():
                if 'WSCAN:' in line:
                    self.assertNotRegex(line.casefold(), r'\b(?:ssid|password|passphrase)\b')
        self.assertEqual(self.patch.patch_directory(self.work), 'already')

    def test_unknown_source_fails_before_any_write(self):
        bad = self.work / 'improvWiFi/ImprovWiFi.cpp'
        bad.write_text(bad.read_text(encoding='utf-8').replace('WiFi.scanNetworks()', 'WiFi.scanNetworks(false)'), encoding='utf-8')
        before = {p: p.read_bytes() for p in self.work.rglob('*') if p.is_file()}
        with self.assertRaises(RuntimeError):
            self.patch.patch_directory(self.work)
        self.assertEqual(before, {p: p.read_bytes() for p in self.work.rglob('*') if p.is_file()})

        shutil.copy2(SOURCE / 'improvWiFi/ImprovWiFi.cpp', bad)
        self.assertEqual(self.patch.patch_directory(self.work), 'changed')
        web_path = self.work / 'basewebserver.cpp'
        web = web_path.read_text(encoding='utf-8')
        for original, drifted in (
            ('const bool pending = _wifiScanTask;',
             'const bool pending = !_wifiScanTask;'),
            ('if (pending) {', 'if (!pending) {'),
            ('if (noNetwork == WIFI_SCAN_RUNNING) return;',
             'if (noNetwork != WIFI_SCAN_RUNNING) return;'),
        ):
            with self.subTest(control_flow=original):
                self.assertEqual(web.count(original), 1)
                web_path.write_text(web.replace(original, drifted), encoding='utf-8')
                before = {p: p.read_bytes() for p in self.work.rglob('*') if p.is_file()}
                with self.assertRaises(RuntimeError):
                    self.patch.patch_directory(self.work)
                self.assertEqual(before, {p: p.read_bytes() for p in self.work.rglob('*') if p.is_file()})

    def test_known_legacy_header_is_upgraded_and_unknown_header_is_preserved(self):
        header = self.work / 'gmt154_wifi_scan_diag.hpp'
        legacy = ROOT / 'test/logging/fixtures/gmt154_wifi_scan_diag_legacy.hpp'
        current = ROOT / 'script/patches/gmt154_wifi_scan_diag.hpp'
        header.write_bytes(legacy.read_bytes())
        self.assertEqual(self.patch.patch_directory(self.work), 'changed')
        self.assertEqual(header.read_bytes(), current.read_bytes())
        self.assertEqual(self.patch.patch_directory(self.work), 'already')

        header.write_text('unknown generated header\n', encoding='utf-8')
        before = {p: p.read_bytes() for p in self.work.rglob('*') if p.is_file()}
        with self.assertRaises(RuntimeError):
            self.patch.patch_directory(self.work)
        self.assertEqual(before, {p: p.read_bytes() for p in self.work.rglob('*') if p.is_file()})

    def test_build_hook_is_registered_only_for_the_diagnostic_environment(self):
        class FakeEnvironment:
            def __init__(self, name):
                self.name = name
                self.middleware = []

            def get(self, key):
                return self.name if key == 'PIOENV' else None

            def subst(self, value):
                return '/tmp/platformio/libdeps'

            def AddBuildMiddleware(self, callback):
                self.middleware.append(callback)

        def load_for(name):
            environment = FakeEnvironment(name)
            runpy.run_path(str(PATCH), init_globals={
                'Import': lambda requested: None,
                'env': environment,
            })
            return environment

        for name in ('gmt154-06', 'gmt154-06-c2b-diag', 'gateway35s3wave-tft-sd'):
            with self.subTest(environment=name):
                self.assertEqual(load_for(name).middleware, [])
        self.assertEqual(len(load_for('gmt154-06-wifi-scan-diag').middleware), 2)

    def test_callback_is_allocation_free_and_each_drain_is_bounded(self):
        compiler = shutil.which('g++')
        if compiler is None:
            self.skipTest('g++ is required for the header runtime contract test')

        temp = Path(self.temp.name)
        stub_dir = temp / 'stubs'
        stub_dir.mkdir()
        for name in ('Arduino.h', 'esp_event.h', 'esp_wifi.h', 'log.hpp'):
            (stub_dir / name).write_text('#pragma once\n', encoding='utf-8')
        preinclude = temp / 'preinclude.hpp'
        preinclude.write_text(textwrap.dedent(r'''\
            #include <cstddef>
            #include <cstdint>
            #include <cstdlib>
            #include <cstring>
            #include <new>

            unsigned allocation_count = 0;
            void* operator new(std::size_t size) {
              ++allocation_count;
              if (void* value = std::malloc(size)) return value;
              throw std::bad_alloc();
            }
            void operator delete(void* value) noexcept { std::free(value); }
            void operator delete(void* value, std::size_t) noexcept { std::free(value); }

            using portMUX_TYPE = int;
            #define portMUX_INITIALIZER_UNLOCKED 0
            inline void portENTER_CRITICAL(portMUX_TYPE*) {}
            inline void portEXIT_CRITICAL(portMUX_TYPE*) {}
            inline std::uint32_t millis() { return 123; }
            #define ESP32 1
            #define GMT154_WIFI_SCAN_DIAG 1
            #define ESP_OK 0
            #define WIFI_EVENT "WIFI_EVENT"
            #define WIFI_EVENT_SCAN_DONE 7
            #define F(value) value
            #define CR "\\n"

            using esp_event_base_t = const char*;
            using esp_err_t = int;
            using esp_event_handler_t = void (*)(void*, esp_event_base_t, std::int32_t, void*);
            struct wifi_event_sta_scan_done_t {
              std::uint8_t scan_id;
              std::uint8_t status;
              std::uint8_t number;
            };
            unsigned registration_count = 0;
            inline esp_err_t esp_event_handler_register(
                esp_event_base_t, std::int32_t, esp_event_handler_t, void*) {
              ++registration_count;
              return ESP_OK;
            }
            struct CapturingLog {
              unsigned event_records = 0;
              unsigned other_logs = 0;
              template <typename... Args>
              void notice(const char* format, Args...) {
                if (std::strstr(format, "WSCAN: event ms=")) ++event_records;
                else ++other_logs;
              }
            };
            CapturingLog Log;
        '''), encoding='utf-8')
        source = temp / 'runtime.cpp'
        source.write_text(textwrap.dedent(r'''\
            #include <gmt154_wifi_scan_diag.hpp>

            int main() {
              gmt154_wifi_diag::drain();
              if (registration_count != 1) return 1;
              Log.event_records = 0;
              wifi_event_sta_scan_done_t event{};
              for (std::uint8_t i = 0; i < 7; ++i) {
                event.scan_id = i;
                event.status = 0;
                event.number = i;
                const unsigned allocations_before = allocation_count;
                const unsigned logs_before = Log.event_records + Log.other_logs;
                gmt154_wifi_diag::on_scan_done(nullptr, WIFI_EVENT,
                                               WIFI_EVENT_SCAN_DONE, &event);
                if (allocation_count != allocations_before) return 2;
                if (Log.event_records + Log.other_logs != logs_before) return 3;
              }
              if (gmt154_wifi_diag::buffer().count != 7) return 4;
              gmt154_wifi_diag::drain();
              if (Log.event_records != 4) return 5;
              if (gmt154_wifi_diag::buffer().count != 3) return 6;
              gmt154_wifi_diag::drain();
              if (Log.event_records != 7) return 7;
              if (gmt154_wifi_diag::buffer().count != 0) return 8;
              if (registration_count != 1) return 9;
              return 0;
            }
        '''), encoding='utf-8')
        executable = temp / 'wifi_scan_diag_runtime.exe'
        build = subprocess.run([
            compiler, '-std=c++17', '-I', str(stub_dir), '-I', str(ROOT / 'script/patches'),
            '-include', str(preinclude), str(source), '-o', str(executable),
        ], capture_output=True, text=True, timeout=30)
        self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
        result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, f'runtime harness returned {result.returncode}: {result.stdout}{result.stderr}')


if __name__ == '__main__':
    unittest.main()
