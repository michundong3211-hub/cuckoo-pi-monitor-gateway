import importlib.util
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).parents[2]
PATCHER_PATH = ROOT / "script" / "patch_gravitymon_gmt154_c3_config_save.py"
MAIN_PATH = ROOT / "src" / "main_gateway.cpp"
PLATFORMIO_PATH = ROOT / "platformio.ini"
ESPFRAMEWORK_WIFI_PATH = (
    ROOT
    / ".pio"
    / "libdeps"
    / "gmt154-06-c3"
    / "espframework"
    / "src"
    / "wificonnection.cpp"
)


def load_patcher():
    if not PATCHER_PATH.is_file():
        return None
    spec = importlib.util.spec_from_file_location(
        "gmt154_c3_config_save_patcher", PATCHER_PATH
    )
    if spec is None or spec.loader is None:
        raise AssertionError("GMT154 C3 config-save patcher could not be loaded")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def extract_braced_block(source, marker):
    start = source.index(marker)
    opening = source.index("{", start)
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start : index + 1]
    raise AssertionError("Unclosed block for " + marker)


class FakePlatformIOEnvironment:
    def __init__(self, pioenv, libdeps):
        self.pioenv = pioenv
        self.libdeps = libdeps
        self.middleware = None
        self.post_action = None

    def get(self, key):
        return self.pioenv if key == "PIOENV" else ""

    def subst(self, value):
        if value != "$PROJECT_LIBDEPS_DIR":
            raise AssertionError(value)
        return str(self.libdeps)

    def AddBuildMiddleware(self, callback):
        self.middleware = callback

    def AddPostAction(self, target, callback):
        if target != "$BUILD_DIR/${PROGNAME}.elf":
            raise AssertionError(target)
        self.post_action = callback


class Gmt154C3WifiRecoveryTest(unittest.TestCase):
    def test_c3_main_loop_gives_lvgl_lower_priority_task_a_tick(self):
        source = MAIN_PATH.read_text(encoding="utf-8")
        loop = extract_braced_block(source, "void loop()")
        loop_without_comments = re.sub(r"//[^\n]*", "", loop)

        self.assertRegex(
            loop_without_comments,
            r"#if\s+defined\(GMT154_06_C3\)\s+delay\(1\);\s+#endif",
            "C3 loopTask must block briefly so priority-0 LVGL can run",
        )

    def test_initial_sta_failure_enters_config_portal_but_success_is_preserved(self):
        source = MAIN_PATH.read_text(encoding="utf-8")
        setup = extract_braced_block(source, "void setup()")

        self.assertRegex(setup, r"if\s*\(\s*!wifiConnected\s*\)\s*\{")
        fallback = setup[setup.index("if (!wifiConnected)") :]
        self.assertIn("runMode = RunMode::wifiSetupMode;", fallback)
        self.assertIn("myWifi.enableImprov(true);", fallback)
        self.assertIn("myWifi.startAP();", fallback)
        fallback_ap = fallback.index("myWifi.startAP();")
        self.assertIn(
            'Log.notice(F("DIAG: MAIN stage=wifi.startAP.begin." CR));',
            fallback[:fallback_ap],
        )
        self.assertIn(
            'Log.notice(F("DIAG: MAIN stage=wifi.startAP.done." CR));',
            fallback[fallback_ap:],
        )
        self.assertIn("if (myWifi.isConnected())", setup)
        self.assertIn("myWebServer.setupWebServer(", setup)
        self.assertIn(
            "#else\n        myWifi.connect(false, WIFI_AP_STA);\n#endif", setup
        )
        self.assertIn(
            "#else\n        myWifi.setAP(myConfig.getWifiDirectSSID(),\n"
            "                     myConfig.getWifiDirectPass());", setup
        )
        self.assertIn("#if defined(GMT154_06_C3)\n      if (!wifiConnected)", setup)

    def test_fallback_decision_uses_connection_state_not_connect_return(self):
        source = MAIN_PATH.read_text(encoding="utf-8")
        setup = extract_braced_block(source, "void setup()")
        c3_start = setup.index("#if defined(GMT154_06_C3)\n        myWifi.connect(")
        c3_end = setup.index("#else", c3_start)
        c3_connect_path = setup[c3_start:c3_end]

        self.assertEqual(c3_connect_path.count("myWifi.isConnected()"), 1)
        self.assertRegex(
            c3_connect_path,
            r"myWifi\.connect\([^;]+\);[\s\S]*myWifi\.isConnected\(\)",
            "C3 must decide from actual STA state after the framework connect call",
        )
        self.assertEqual(
            setup.count("wifiConnected = myWifi.isConnected();"),
            2,
            "both C3 Wi-Fi paths must preserve successful STA connections",
        )

    def test_pinned_framework_connect_semantics_are_exercised_with_controlled_wifi(self):
        compiler = shutil.which("g++") or shutil.which("clang++")
        self.assertIsNotNone(compiler, "host C++ compiler is required for this regression")
        self.assertTrue(
            ESPFRAMEWORK_WIFI_PATH.is_file(),
            "build the gmt154-06-c3 PlatformIO environment before this dependency regression",
        )
        dependency = ESPFRAMEWORK_WIFI_PATH.read_text(encoding="utf-8")
        connect_definition = extract_braced_block(
            dependency, "bool WifiConnection::connect(bool wifiDirect, wifi_mode_t mode)"
        )
        harness = r'''#include <cassert>
#include <cstring>
#include <string>
#define F(value) value
#define CR "\n"
using wifi_mode_t = int;
constexpr int WIFI_AP = 1;
constexpr int WIFI_AP_STA = 2;
struct MockConfig {
  const char* getWifiDirectSSID() { return ""; }
  const char* getWifiDirectPass() { return ""; }
  const char* getWifiSSID(int index) { return index == 0 ? "primary" : ""; }
  const char* getWifiPass(int) { return "secret"; }
  int getWifiConnectionTimeout() { return 5; }
};
struct MockLog {
  template <typename... Args> void notice(const char*, Args...) {}
  template <typename... Args> void warning(const char*, Args...) {}
};
static MockLog Log;
class WifiConnection {
 public:
  explicit WifiConnection(bool stationWillConnect) : stationWillConnect(stationWillConnect) {}
  MockConfig* _wifiConfig = &config;
  MockConfig config;
  bool stationWillConnect;
  bool stationConnected = false;
  void connectAsync(const char*, const char*, wifi_mode_t) {}
  bool waitForConnection(int) { stationConnected = stationWillConnect; return stationWillConnect; }
  bool isConnected() { return stationConnected; }
  bool connect(bool wifiDirect, wifi_mode_t mode);
};
''' + connect_definition + r'''
int main() {
  WifiConnection firstSsidSucceeds(true);
  const bool returned = firstSsidSucceeds.connect(false, WIFI_AP);
  assert(!returned); // Pinned framework falls through to `return false` here.
  assert(firstSsidSucceeds.isConnected());

  WifiConnection firstSsidFails(false);
  const bool failureReturned = firstSsidFails.connect(false, WIFI_AP);
  assert(!failureReturned);
  assert(!firstSsidFails.isConnected());
  return 0;
}
'''
        with tempfile.TemporaryDirectory() as temp_dir:
            source_path = Path(temp_dir) / "wifi_connect_semantics.cpp"
            executable_path = Path(temp_dir) / "wifi_connect_semantics.exe"
            source_path.write_text(harness, encoding="utf-8")
            compile_result = subprocess.run(
                [compiler, "-std=c++11", str(source_path), "-o", str(executable_path)],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(compile_result.returncode, 0, compile_result.stderr)
            run_result = subprocess.run(
                [str(executable_path)], capture_output=True, text=True, check=False
            )
            self.assertEqual(run_result.returncode, 0, run_result.stderr)

    def test_setup_mode_does_not_repeat_blocking_station_connect(self):
        source = MAIN_PATH.read_text(encoding="utf-8")
        loop = extract_braced_block(source, "void loop()")
        setup_case = extract_braced_block(loop, "case RunMode::wifiSetupMode:")

        self.assertIn("break;", setup_case)
        self.assertNotIn("myWifi.connect(", setup_case)
        self.assertEqual(loop.count("myWifi.connect()"), 1)

    def test_c3_target_alone_registers_config_save_patcher(self):
        source = PLATFORMIO_PATH.read_text(encoding="utf-8")
        section = source.split("[env:gmt154-06-c3]", 1)[1].split("\n[", 1)[0]

        self.assertIn("pre:script/patch_gravitymon_gmt154_c3_config_save.py", section)
        self.assertNotIn(
            "pre:script/patch_gravitymon_gmt154_c3_config_save.py",
            source.split("[env:gmt154-06-c3]", 1)[0],
        )

    def test_config_save_patch_reports_failure_and_keeps_success_only_after_save(self):
        patcher = load_patcher()
        self.assertIsNotNone(patcher, "C3 config-save patcher is not implemented yet")

        success_response = '''
  AsyncJsonResponse *response = new AsyncJsonResponse(false);
  obj = response->getRoot().as<JsonObject>();
  obj[PARAM_SUCCESS] = true;
  obj[PARAM_MESSAGE] = "Configuration updated";
  response->setLength();
  request->send(response);
  PERF_END("webserver-api-config-write");'''
        original = (
            "prefix\n" + patcher.ORIGINAL_CONFIG_WRITE + success_response + "\nsuffix\n"
        )
        with tempfile.TemporaryDirectory() as temp_dir:
            source_path = Path(temp_dir) / "web_brewing.cpp"
            source_path.write_text(original, encoding="utf-8")

            self.assertEqual(patcher.patch_source(source_path), "changed")
            patched = source_path.read_text(encoding="utf-8")
            failure = patched.index("if (!_webConfig->saveFile())")
            failure_response = patched.index("obj[PARAM_SUCCESS] = false;", failure)
            error_status = patched.index("response->setCode(500);", failure)
            return_from_failure = patched.index("return;", error_status)
            success = patched.index("obj[PARAM_SUCCESS] = true;", failure)
            self.assertLess(failure, failure_response)
            self.assertLess(failure_response, error_status)
            self.assertLess(error_status, return_from_failure)
            self.assertLess(return_from_failure, success)
            self.assertEqual(patcher.patch_source(source_path), "already")

    def test_config_save_patch_fails_closed_on_unknown_source(self):
        patcher = load_patcher()
        self.assertIsNotNone(patcher, "C3 config-save patcher is not implemented yet")
        with tempfile.TemporaryDirectory() as temp_dir:
            source_path = Path(temp_dir) / "web_brewing.cpp"
            source_path.write_text("unknown dependency version\n", encoding="utf-8")
            before = source_path.read_bytes()

            with self.assertRaises(patcher.PatchError):
                patcher.patch_source(source_path)

            self.assertEqual(source_path.read_bytes(), before)

    def test_platformio_post_action_accepts_scons_env_keyword(self):
        patcher = load_patcher()
        self.assertIsNotNone(patcher, "C3 config-save patcher is not implemented yet")
        with tempfile.TemporaryDirectory() as temp_dir:
            libdeps = Path(temp_dir) / "libdeps"
            source_path = (
                libdeps
                / patcher.TARGET_ENV
                / "gravitymon"
                / "src"
                / "web_brewing.cpp"
            )
            source_path.parent.mkdir(parents=True)
            source_path.write_text(
                "prefix\n" + patcher.ORIGINAL_CONFIG_WRITE + "\nsuffix\n",
                encoding="utf-8",
            )
            env = FakePlatformIOEnvironment(patcher.TARGET_ENV, libdeps)

            with mock.patch.object(patcher, "Import", lambda name: None, create=True):
                with mock.patch.object(patcher, "env", env, create=True):
                    patcher._register_platformio_hook()

            self.assertIsNotNone(env.middleware)
            self.assertIsNotNone(env.post_action)
            env.post_action(target=None, source=None, env=env)


if __name__ == "__main__":
    unittest.main(verbosity=2)
