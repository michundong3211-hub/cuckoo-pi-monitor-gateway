"""Compile the actual espframework logger/sink against a host capture device."""
import importlib.util
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
PATCH = ROOT / "script/patch_espframework_gmt154_logging.py"


class LoggingRuntimeTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="gmt154-log-test-")
        cls.work = Path(cls.temp.name)
        shutil.copytree(ROOT / "test/logging/stubs", cls.work / "stubs")
        shutil.copy2(ROOT / "test/logging/logging_runtime.cpp", cls.work / "logging_runtime.cpp")
        for name in ("gmt154_heap_diagnostics.cpp", "gmt154_heap_diagnostics.hpp"):
            shutil.copy2(ROOT / "src" / name, cls.work / name)
        source = ROOT / ".pio/libdeps/gmt154-06/espframework/src"
        for name in ("ArduinoLog.hpp", "ArduinoLog.cpp", "serialws.hpp", "serialws.cpp"):
            shutil.copy2(source / name, cls.work / name)
        if PATCH.exists():
            spec = importlib.util.spec_from_file_location("logging_patch", PATCH)
            cls.patch = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(cls.patch)
            cls.patch.patch_directory(cls.work)
        display = (ROOT / "src/display.cpp").read_text(encoding="utf-8-sig")
        match = re.search(r'Log.notice\(F\("DIAG: GMT154 task result=.*?;', display, re.S)
        if not match:
            raise RuntimeError("display diagnostic call not found")
        # Run the production call and its casts with fixed independently checked inputs.
        (cls.work / "display_log_case.inc").write_text(
            "void runDisplayLogCase() {\n"
            "int task_result = 1; uint32_t now_ms = 123;\n"
            "uint32_t internal_free_before_task = 57008, internal_largest_before_task = 31732;\n"
            "uint32_t internal_free_after_task = 46000, internal_largest_after_task = 20000;\n"
            "auto gmt154FirstFrameElapsedMs = [](uint32_t n) { return uint64_t(n); };\n"
            + match.group(0) + "\n}\n", encoding="utf-8")
        cls.exe = cls.work / "logging-test.exe"
        command = ["g++", "-std=c++17", "-pthread", "-DGMT154_06=1",
                   "-I" + str(cls.work / "stubs"), "-I" + str(cls.work),
                   str(cls.work / "logging_runtime.cpp"),
                   str(cls.work / "ArduinoLog.cpp"), str(cls.work / "serialws.cpp"),
                   str(cls.work / "gmt154_heap_diagnostics.cpp"),
                   "-o", str(cls.exe)]
        result = subprocess.run(command, capture_output=True, text=True)
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def run_case(self, name):
        result = subprocess.run([str(self.exe), name], capture_output=True, text=True, timeout=20)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_diagnostic_values(self):
        self.run_case("format")

    def test_full_buffer_before_next_write(self):
        self.run_case("boundary")

    def test_concurrent_records(self):
        self.run_case("threads")

    def test_heap_diagnostics_and_rate_limit(self):
        self.run_case("heap")


if __name__ == "__main__":
    unittest.main()
