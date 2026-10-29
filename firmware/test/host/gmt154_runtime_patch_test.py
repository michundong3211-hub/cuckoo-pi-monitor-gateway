import importlib.util
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).parents[2]
PATCHER_PATH = ROOT / "script" / "patch_espframework_gmt154_runtime.py"

ORIGINAL_RESET = '''void forcedReset() {
#if !defined(ESP8266) && ESP_ARDUINO_VERSION_MAJOR >= 3
  esp_task_wdt_config_t task = {1000, 0, false};

  ledOff();
  LittleFS.end();
  delay(100);
  esp_task_wdt_init(&task);
  esp_task_wdt_add(NULL);
  while (true) {
    // wait for watchdog timer to be triggered
  }
#elif !defined(ESP8266) && ESP_ARDUINO_VERSION_MAJOR >= 2 && \\
    ESP_ARDUINO_VERSION_MAJOR < 3
  ledOff();
  LittleFS.end();
  delay(100);
  esp_task_wdt_init(1, true);
  esp_task_wdt_add(NULL);
  while (true) {
    // wait for watchdog timer to be triggered
  }
#endif
}'''

PATCHED_RESET = '''void forcedReset() {
#if defined(GMT154_06) && defined(ESP32)
  ledOff();
  LittleFS.end();
  delay(100);
  esp_restart();
#elif !defined(ESP8266) && ESP_ARDUINO_VERSION_MAJOR >= 3
  esp_task_wdt_config_t task = {1000, 0, false};

  ledOff();
  LittleFS.end();
  delay(100);
  esp_task_wdt_init(&task);
  esp_task_wdt_add(NULL);
  while (true) {
    // wait for watchdog timer to be triggered
  }
#elif !defined(ESP8266) && ESP_ARDUINO_VERSION_MAJOR >= 2 && \\
    ESP_ARDUINO_VERSION_MAJOR < 3
  ledOff();
  LittleFS.end();
  delay(100);
  esp_task_wdt_init(1, true);
  esp_task_wdt_add(NULL);
  while (true) {
    // wait for watchdog timer to be triggered
  }
#endif
}'''

ORIGINAL_WIFI_LOG = '''    Log.notice(F("WIFI: Connecting to wifi using stored settings %s." CR),
               ssid);'''
PATCHED_WIFI_LOG = '''    Log.notice(F("WIFI: Connecting to wifi using stored settings." CR));'''

ORIGINAL_UTILS = "#include <utils.hpp>\n\n" + ORIGINAL_RESET + "\n"
ORIGINAL_WIFI = "#include <wificonnection.hpp>\n\n" + ORIGINAL_WIFI_LOG + "\n"


def load_patcher():
    spec = importlib.util.spec_from_file_location("gmt154_runtime_patcher", PATCHER_PATH)
    if spec is None or spec.loader is None:
        raise AssertionError("GMT154 runtime patch module could not be loaded")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


PATCHER = load_patcher()


class RuntimePatchTest(unittest.TestCase):
    def setUp(self):
        self.tempdir = tempfile.TemporaryDirectory()
        self.root = Path(self.tempdir.name)
        self.src = self.root / "espframework" / "src"
        self.src.mkdir(parents=True)

    def tearDown(self):
        self.tempdir.cleanup()

    def write_pair(self, utils=ORIGINAL_UTILS, wifi=ORIGINAL_WIFI, newline="\n"):
        utils_path = self.src / "utils.cpp"
        wifi_path = self.src / "wificonnection.cpp"
        utils_path.write_bytes(utils.replace("\n", newline).encode("utf-8"))
        wifi_path.write_bytes(wifi.replace("\n", newline).encode("utf-8"))
        return utils_path, wifi_path

    def test_original_sources_are_patched_and_wifi_ssid_is_not_logged(self):
        utils_path, wifi_path = self.write_pair(newline="\r\n")

        result = PATCHER.patch_directory(self.src)

        self.assertEqual(result, "changed")
        utils = utils_path.read_bytes()
        wifi = wifi_path.read_bytes()
        self.assertIn(b"#include <esp_system.h>\r\n", utils)
        self.assertIn(PATCHED_RESET.encode().replace(b"\n", b"\r\n"), utils)
        self.assertIn(PATCHED_WIFI_LOG.encode().replace(b"\n", b"\r\n"), wifi)
        self.assertNotIn(b"ssid);", wifi)
        self.assertNotIn(b"stored settings %s", wifi)
        self.assertNotIn(b"\n", utils.replace(b"\r\n", b""))
        self.assertNotIn(b"\n", wifi.replace(b"\r\n", b""))

    def test_complete_patch_is_idempotent(self):
        utils_path, wifi_path = self.write_pair()
        self.assertEqual(PATCHER.patch_directory(self.src), "changed")
        patched = (utils_path.read_bytes(), wifi_path.read_bytes())

        result = PATCHER.patch_directory(self.src)

        self.assertEqual(result, "already")
        self.assertEqual((utils_path.read_bytes(), wifi_path.read_bytes()), patched)

    def test_unknown_source_fails_before_either_file_is_written(self):
        unknown_utils = ORIGINAL_UTILS.replace("delay(100);", "delay(250);")
        utils_path, wifi_path = self.write_pair(utils=unknown_utils)
        before = (utils_path.read_bytes(), wifi_path.read_bytes())

        with self.assertRaises(PATCHER.PatchError):
            PATCHER.patch_directory(self.src)

        self.assertEqual((utils_path.read_bytes(), wifi_path.read_bytes()), before)

    def test_mixed_original_and_patched_sources_are_rejected_without_writes(self):
        partial_utils = ORIGINAL_UTILS.replace(ORIGINAL_RESET, PATCHED_RESET).replace(
            "#include <utils.hpp>",
            "#include <utils.hpp>\n#include <esp_system.h>",
        )
        utils_path, wifi_path = self.write_pair(utils=partial_utils)
        before = (utils_path.read_bytes(), wifi_path.read_bytes())

        with self.assertRaises(PATCHER.PatchError):
            PATCHER.patch_directory(self.src)

        self.assertEqual((utils_path.read_bytes(), wifi_path.read_bytes()), before)

    def test_patched_forced_reset_runs_cleanup_then_restart_without_wdt(self):
        utils_path, _ = self.write_pair()
        PATCHER.patch_directory(self.src)
        compiler = shutil.which("g++")
        self.assertIsNotNone(compiler, "g++ is required for the host runtime fake")

        (self.root / "utils.hpp").write_text(
            """#pragma once
#include <stdexcept>
#include <string>
#include <vector>
inline std::vector<std::string> runtime_events;
struct FakeFs { void end() { runtime_events.emplace_back(\"LittleFS.end\"); } };
inline FakeFs LittleFS;
inline void ledOff() { runtime_events.emplace_back(\"ledOff\"); }
inline void delay(int ms) { runtime_events.emplace_back(\"delay:\" + std::to_string(ms)); }
struct esp_task_wdt_config_t { int timeout; int idle_mask; bool panic; };
struct WatchdogReached {};
inline void esp_task_wdt_init(const esp_task_wdt_config_t*) { runtime_events.emplace_back(\"wdt_init\"); }
inline void esp_task_wdt_init(int, bool) { runtime_events.emplace_back(\"wdt_init\"); }
inline void esp_task_wdt_add(void*) { runtime_events.emplace_back(\"wdt_add\"); throw WatchdogReached{}; }
""",
            encoding="utf-8",
        )
        (self.root / "esp_system.h").write_text(
            """#pragma once
#include <utils.hpp>
inline void esp_restart() { runtime_events.emplace_back(\"esp_restart\"); }
""",
            encoding="utf-8",
        )
        executable = self.root / "patched-reset.exe"
        program = utils_path.read_text(encoding="utf-8") + """
int main() {
  forcedReset();
  const std::vector<std::string> expected = {
      \"ledOff\", \"LittleFS.end\", \"delay:100\", \"esp_restart\"};
  return runtime_events == expected ? 0 : 11;
}
"""
        (self.root / "patched_reset_test.cpp").write_text(program, encoding="utf-8")
        compile_result = subprocess.run(
            [compiler, "-std=c++17", "-DGMT154_06=1", "-DESP32=1",
             "-DESP_ARDUINO_VERSION_MAJOR=3", "-I", str(self.root),
             str(self.root / "patched_reset_test.cpp"), "-o", str(executable)],
            text=True, capture_output=True, check=False,
        )
        self.assertEqual(compile_result.returncode, 0, compile_result.stdout + compile_result.stderr)
        run_result = subprocess.run([str(executable)], check=False)
        self.assertEqual(run_result.returncode, 0)

    def test_original_forced_reset_fake_catches_watchdog_before_loop(self):
        self.write_pair()
        compiler = shutil.which("g++")
        self.assertIsNotNone(compiler, "g++ is required for the host runtime fake")
        (self.root / "utils.hpp").write_text(
            """#pragma once
#include <stdexcept>
#include <string>
#include <vector>
inline std::vector<std::string> runtime_events;
struct FakeFs { void end() { runtime_events.emplace_back(\"LittleFS.end\"); } };
inline FakeFs LittleFS;
inline void ledOff() { runtime_events.emplace_back(\"ledOff\"); }
inline void delay(int ms) { runtime_events.emplace_back(\"delay:\" + std::to_string(ms)); }
struct esp_task_wdt_config_t { int timeout; int idle_mask; bool panic; };
struct WatchdogReached {};
inline void esp_task_wdt_init(const esp_task_wdt_config_t*) { runtime_events.emplace_back(\"wdt_init\"); }
inline void esp_task_wdt_init(int, bool) { runtime_events.emplace_back(\"wdt_init\"); }
inline void esp_task_wdt_add(void*) { runtime_events.emplace_back(\"wdt_add\"); throw WatchdogReached{}; }
""",
            encoding="utf-8",
        )
        source = "#include <utils.hpp>\n" + ORIGINAL_RESET + """
int main() {
  try { forcedReset(); } catch (const WatchdogReached&) {}
  const std::vector<std::string> expected = {
      \"ledOff\", \"LittleFS.end\", \"delay:100\", \"wdt_init\", \"wdt_add\"};
  return runtime_events == expected ? 0 : 12;
}
"""
        source_path = self.root / "original_reset_test.cpp"
        source_path.write_text(source, encoding="utf-8")
        executable = self.root / "original-reset.exe"
        compile_result = subprocess.run(
            [compiler, "-std=c++17", "-DESP_ARDUINO_VERSION_MAJOR=3", "-I",
             str(self.root), str(source_path), "-o", str(executable)],
            text=True, capture_output=True, check=False,
        )
        self.assertEqual(compile_result.returncode, 0, compile_result.stdout + compile_result.stderr)
        run_result = subprocess.run([str(executable)], check=False)
        self.assertEqual(run_result.returncode, 0)

    def test_only_target_environment_registers_the_build_hook(self):
        class FakeEnvironment:
            def __init__(self, pioenv, libdeps):
                self.pioenv = pioenv
                self.libdeps = libdeps
                self.middleware = None

            def get(self, key):
                return self.pioenv if key == "PIOENV" else ""

            def subst(self, value):
                if value != "$PROJECT_LIBDEPS_DIR":
                    raise AssertionError(value)
                return str(self.libdeps)

            def AddBuildMiddleware(self, callback):
                self.middleware = callback

        class FakeNode:
            def __init__(self, path):
                self.path = path

            def srcnode(self):
                return self

            def get_path(self):
                return str(self.path)

        libdeps = self.root / "libdeps"
        target_src = libdeps / "gmt154-06" / "espframework" / "src"
        target_src.mkdir(parents=True)
        (target_src / "utils.cpp").write_text(ORIGINAL_UTILS, encoding="utf-8")
        (target_src / "wificonnection.cpp").write_text(ORIGINAL_WIFI, encoding="utf-8")

        for pioenv in ("gateway32s3wave-tft-sd", "gmt154-06-c2b-diag"):
            fake = FakeEnvironment(pioenv, libdeps)
            PATCHER.Import = lambda name: None
            PATCHER.env = fake
            try:
                PATCHER._register_platformio_hook()
            finally:
                del PATCHER.Import
                del PATCHER.env
            self.assertIsNone(fake.middleware, pioenv)

        fake = FakeEnvironment("gmt154-06", libdeps)
        PATCHER.Import = lambda name: None
        PATCHER.env = fake
        try:
            PATCHER._register_platformio_hook()
        finally:
            del PATCHER.Import
            del PATCHER.env
        self.assertIsNotNone(fake.middleware)
        node = FakeNode(target_src / "utils.cpp")
        self.assertIs(fake.middleware(fake, node), node)
        self.assertIn(PATCHED_RESET, (target_src / "utils.cpp").read_text(encoding="utf-8"))
        self.assertIn(PATCHED_WIFI_LOG, (target_src / "wificonnection.cpp").read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main(verbosity=2)
