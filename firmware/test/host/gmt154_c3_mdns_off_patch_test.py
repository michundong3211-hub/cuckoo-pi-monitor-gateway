import importlib.util
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).parents[2]
PATCHER_PATH = ROOT / "script" / "patch_espframework_gmt154_c3_mdns_off.py"
PLATFORMIO_PATH = ROOT / "platformio.ini"
MAIN_PATH = ROOT / "src" / "main_gateway.cpp"
PINNED_SOURCE_ROOT = (
    ROOT / ".pio" / "libdeps" / "gmt154-06-c3" / "espframework" / "src"
)
SOURCE_NAMES = ("basewebserver.cpp", "basewebserver2.cpp")
GRAVITYMON_SOURCE = (
    ROOT / ".pio" / "libdeps" / "gmt154-06-c3" / "gravitymon" / "src" / "web_brewing.cpp"
)


def load_patcher():
    if not PATCHER_PATH.is_file():
        return None
    spec = importlib.util.spec_from_file_location(
        "gmt154_c3_mdns_off_patcher", PATCHER_PATH
    )
    if spec is None or spec.loader is None:
        raise AssertionError("C3 mDNS patcher could not be loaded")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class Gmt154C3MdnsOffPatchTest(unittest.TestCase):
    def test_patch_targets_both_pinned_backend_source_blocks_and_is_idempotent(self):
        patcher = load_patcher()
        self.assertIsNotNone(patcher, "C3 mDNS patcher is not implemented yet")
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_root = Path(temp_dir)
            for name in SOURCE_NAMES:
                original_path = PINNED_SOURCE_ROOT / name
                self.assertTrue(original_path.is_file(), f"missing pinned source: {name}")
                original = original_path.read_bytes()
                original_text = original.decode("utf-8").replace("\r\n", "\n")
                self.assertEqual(original_text.count(patcher.ORIGINAL_MDNS_BLOCK), 1)
                test_copy = temp_root / name
                test_copy.write_bytes(original)

                self.assertEqual(patcher.patch_source(test_copy), "changed")
                patched = test_copy.read_text(encoding="utf-8")
                self.assertEqual(patcher.patch_source(test_copy), "already")
                self.assertIn(patcher.PATCHED_MDNS_BLOCK, patched)
                self.assertEqual(patched.count("MDNS.begin("), 1)
                self.assertEqual(patched.count('MDNS.addService("http", "tcp", 80);'), 1)
                if name == "basewebserver2.cpp":
                    self.assertIn("_server->listen(80);", patched)
                else:
                    self.assertIn("_server->begin();", patched)
                self.assertIn("setupWebHandlers();", patched)

    def test_partial_or_unknown_backend_source_fails_closed(self):
        patcher = load_patcher()
        self.assertIsNotNone(patcher, "C3 mDNS patcher is not implemented yet")
        with tempfile.TemporaryDirectory() as temp_dir:
            source = Path(temp_dir) / "basewebserver.cpp"
            source.write_text(
                "void setup() {\n"
                + patcher.ORIGINAL_MDNS_BLOCK.replace(
                    'MDNS.addServiceTxt("http", "tcp", "id", _webConfig->getID());',
                    "// unknown modified block",
                )
                + "\n}\n",
                encoding="utf-8",
            )
            before = source.read_bytes()
            with self.assertRaises(patcher.PatchError):
                patcher.patch_source(source)
            self.assertEqual(source.read_bytes(), before)

    def test_mdns_registration_in_gravitymon_backend_is_disabled_idempotently(self):
        patcher = load_patcher()
        self.assertTrue(
            hasattr(patcher, "patch_gravitymon_source"),
            "mDNS-off hook does not cover gravitymon web_brewing.cpp",
        )
        self.assertTrue(GRAVITYMON_SOURCE.is_file())
        original = GRAVITYMON_SOURCE.read_text(encoding="utf-8").replace("\r\n", "\n")
        self.assertEqual(original.count(patcher.ORIGINAL_GRAVITYMON_MDNS_BLOCK), 1)

        with tempfile.TemporaryDirectory() as temp_dir:
            source = Path(temp_dir) / "web_brewing.cpp"
            source.write_text(original, encoding="utf-8")
            self.assertEqual(patcher.patch_gravitymon_source(source), "changed")
            patched = source.read_text(encoding="utf-8")
            self.assertIn(patcher.PATCHED_GRAVITYMON_MDNS_BLOCK, patched)
            self.assertEqual(patcher.patch_gravitymon_source(source), "already")
            for token in (
                'MDNS.addService(serviceName, "tcp", 80);',
                'MDNS.addServiceTxt(serviceName, "tcp", "ver", CFG_APPVER);',
                'MDNS.addServiceTxt(serviceName, "tcp", "app", CFG_APPNAME);',
                'MDNS.addServiceTxt(serviceName, "tcp", "id", _webConfig->getID());',
            ):
                self.assertEqual(patched.count(token), 1)
            self.assertNotIn(
                "#if !defined(GMT154_C3_MDNS_OFF_DIAG)\n"
                "  BaseWebServer::setupWebServer();\n",
                patched,
            )

    def test_ble_init_off_env_inherits_mdns_off_and_adds_only_its_diagnostic_macro(self):
        platform = PLATFORMIO_PATH.read_text(encoding="utf-8")
        marker = "[env:gmt154-06-c3-mdns-off-ble-init-off]"
        self.assertIn(marker, platform)
        section = platform.split(marker, 1)[1].split("\n[", 1)[0]
        self.assertIn("extends = env:gmt154-06-c3-mdns-off", section)
        self.assertIn("GMT154_06_BLE_INIT_OFF_DIAG=1", section)
        self.assertNotIn("GMT154_C3_MDNS_OFF_DIAG", section)
        self.assertNotIn("extra_scripts =", section)

        main = MAIN_PATH.read_text(encoding="utf-8")
        guard = (
            "#if defined(GMT154_06) && defined(GMT154_06_BLE_INIT_OFF_DIAG) && \\\n"
            "    GMT154_06_BLE_INIT_OFF_DIAG == 1\n"
            "    Log.notice(F(\"DIAG: GMT154 BLE scanner init skipped.\" CR));\n"
            "#else\n    bleScanner.init();\n#endif"
        )
        self.assertIn(guard, main)
        self.assertEqual(main.count("bleScanner.init();"), 1)
        ble = (ROOT / "src" / "ble_gateway.cpp").read_text()
        scan = ble.split("bool BleScanner::scan()", 1)[1].split(
            "void BleScanner::proccesTiltBeacon", 1
        )[0]
        self.assertTrue(scan.lstrip().startswith("{\n  if (!_bleScan) return false;"))
        loop = ble.split("void BleScanner::loop(int sdLogMinTime)", 1)[1].split(
            "void BleScanner::addData", 1
        )[0]
        self.assertIn("while (!_bleData.empty())", loop)
        self.assertNotIn("_bleScan", loop)
        self.assertNotIn("NimBLE", loop)

    def test_diagnostic_environment_is_c3_only_and_inherits_normal_features(self):
        platform = PLATFORMIO_PATH.read_text(encoding="utf-8")
        marker = "[env:gmt154-06-c3-mdns-off]"
        self.assertIn(marker, platform)
        section = platform.split(marker, 1)[1].split("\n[", 1)[0]
        self.assertIn("extends = env:gmt154-06-c3", section)
        self.assertIn("pre:script/patch_espframework_gmt154_c3_mdns_off.py", section)
        self.assertIn("GMT154_C3_MDNS_OFF_DIAG=1", section)
        self.assertNotIn("patch_espframework_gmt154_c3_mdns_off.py", platform.split(marker, 1)[0])
        self.assertNotIn("lib_deps =", section)
        self.assertNotIn("lib_ignore =", section)
        main = MAIN_PATH.read_text(encoding="utf-8")
        self.assertIn("bleScanner.init();", main)
        self.assertIn("myWebServer.setupWebServer(", main)
        self.assertIn("myWifi.isConnected()", main)


if __name__ == "__main__":
    unittest.main(verbosity=2)
