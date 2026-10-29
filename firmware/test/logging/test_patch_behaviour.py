import importlib.util
from pathlib import Path
import shutil
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    "logging_patch", ROOT / "script/patch_espframework_gmt154_logging.py")
patch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(patch)


class PatchBehaviourTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="gmt154-patch-test-")
        self.directory = Path(self.temp.name)
        for name in ("ArduinoLog.hpp", "serialws.cpp"):
            shutil.copy2(ROOT / ".pio/libdeps/gmt154-06/espframework/src" / name,
                         self.directory / name)

    def tearDown(self):
        self.temp.cleanup()

    def test_repeat_patch_preserves_bytes(self):
        patch.patch_directory(self.directory)
        before = {p.name: p.read_bytes() for p in self.directory.iterdir()}
        self.assertEqual(patch.patch_directory(self.directory), "already")
        self.assertEqual(before, {p.name: p.read_bytes() for p in self.directory.iterdir()})

    def test_unknown_dependency_fails_without_partial_edits(self):
        target = self.directory / "serialws.cpp"
        target.write_text("unexpected dependency revision", encoding="utf-8")
        before = {p.name: p.read_bytes() for p in self.directory.iterdir()}
        with self.assertRaises(RuntimeError):
            patch.patch_directory(self.directory)
        self.assertEqual(before, {p.name: p.read_bytes() for p in self.directory.iterdir()})

    def test_hook_accepts_c2b_and_uses_c2b_dependency_directory(self):
        source = (ROOT / "script/patch_espframework_gmt154_logging.py").read_text()
        self.assertIn('"gmt154-06-c2b-diag"', source)
        self.assertIn('if pioenv not in TARGET_ENVS:', source)
        self.assertIn('f"{pioenv}/espframework/src"', source)

        platformio = (ROOT / "platformio.ini").read_text(encoding="utf-8")
        diag_env = platformio.split("[env:gmt154-06-wifi-scan-diag]", 1)[1].split("\n[", 1)[0]
        self.assertIn("pre:script/patch_espframework_gmt154_logging.py", diag_env)
        self.assertIn("gmt154-06-wifi-scan-diag", patch.TARGET_ENVS)

        # Exercise the logging hook against the real diagnostic dependency copy,
        # but keep all test writes isolated from .pio/libdeps.
        libdeps = Path(self.temp.name) / "diag-libdeps"
        diag_source = (
            ROOT / ".pio/libdeps/gmt154-06-wifi-scan-diag/espframework/src"
        )
        diag_directory = libdeps / "gmt154-06-wifi-scan-diag/espframework/src"
        diag_directory.mkdir(parents=True)
        for name in ("ArduinoLog.hpp", "serialws.cpp"):
            shutil.copy2(diag_source / name, diag_directory / name)

        class FakeDiagnosticEnvironment:
            middleware = None

            def get(self, key):
                return "gmt154-06-wifi-scan-diag" if key == "PIOENV" else ""

            def subst(self, value):
                if value != "$PROJECT_LIBDEPS_DIR":
                    raise AssertionError(value)
                return str(libdeps)

            def AddBuildMiddleware(self, callback):
                self.middleware = callback

        fake_env = FakeDiagnosticEnvironment()
        patch.Import = lambda name: None
        patch.env = fake_env
        patch.register_hook()
        self.assertIsNotNone(fake_env.middleware)
        fake_env.middleware(fake_env, object())
        serialws = (diag_directory / "serialws.cpp").read_text(encoding="utf-8")
        self.assertEqual(serialws.count("Gmt154LogGuard log_guard;"), 2)
        self.assertIn("if (_bufSize >= sizeof(_buf)) flush();", serialws)
        self.assertIn("_webSocket->textAll(reinterpret_cast<const char *>(chunk), count);", serialws)

    def test_c2c_hook_patches_active_dependency_directory(self):
        libdeps = Path(self.temp.name) / "libdeps"
        c2c_directory = (
            libdeps / "gmt154-06-c2c-diag" / "espframework" / "src"
        )
        c2c_directory.mkdir(parents=True)
        for name in ("ArduinoLog.hpp", "serialws.cpp"):
            shutil.copy2(self.directory / name, c2c_directory / name)

        class FakeEnvironment:
            middleware = None

            def get(self, key):
                return "gmt154-06-c2c-diag" if key == "PIOENV" else ""

            def subst(self, value):
                if value != "$PROJECT_LIBDEPS_DIR":
                    raise AssertionError(value)
                return str(libdeps)

            def AddBuildMiddleware(self, callback):
                self.middleware = callback

        fake_env = FakeEnvironment()
        patch.Import = lambda name: None
        patch.env = fake_env
        try:
            patch.register_hook()
            self.assertIsNotNone(fake_env.middleware)
            fake_env.middleware(fake_env, object())
        finally:
            del patch.Import
            del patch.env

        self.assertIn(
            '#include "gmt154_logging_guard.hpp"',
            (c2c_directory / "ArduinoLog.hpp").read_text(),
        )
        self.assertTrue((c2c_directory / "gmt154_logging_guard.hpp").exists())
        self.assertFalse(
            (libdeps / "gmt154-06-c2b-diag" / "espframework" / "src").exists()
        )

    def test_scons_script_does_not_require_dunder_file(self):
        source = (ROOT / "script/patch_espframework_gmt154_logging.py").read_text()
        self.assertIn('if "__file__" in globals() else Path.cwd() / "script"', source)


if __name__ == "__main__":
    unittest.main()
