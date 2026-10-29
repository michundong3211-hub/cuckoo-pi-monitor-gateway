import unittest
from pathlib import Path


ROOT = Path(__file__).parents[2]
PLATFORMIO = ROOT / "platformio.ini"
BLE_SOURCE = ROOT / "src" / "ble_gateway.cpp"


class Gmt154C3BleStageDiagnosticContractTest(unittest.TestCase):
    def test_diagnostic_environment_keeps_ble_and_inherits_mdns_off_c3(self):
        config = PLATFORMIO.read_text(encoding="utf-8")
        marker = "[env:gmt154-06-c3-mdns-off-ble-stage-diag]"
        if marker not in config:
            self.fail("BLE stage diagnostic environment is missing")
        section = config.split(marker, 1)[1].split("\n[", 1)[0]
        self.assertIn("extends = env:gmt154-06-c3-mdns-off", section)
        self.assertIn("GMT154_C3_BLE_STAGE_DIAG=1", section)
        self.assertNotIn("GMT154_06_BLE_INIT_OFF_DIAG", section)
        self.assertNotIn("extra_scripts =", section)
        self.assertNotIn("lib_deps =", section)

    def test_stage_snapshots_cover_init_configuration_and_first_scan_result(self):
        source = BLE_SOURCE.read_text(encoding="utf-8")
        self.assertIn('#include <gmt154_heap_diagnostics.hpp>', source)
        init = source.split("void BleScanner::init()", 1)[1].split(
            "void BleScanner::deInit()", 1
        )[0]
        scan = source.split("bool BleScanner::scan()", 1)[1].split(
            "void BleScanner::proccesTiltBeacon", 1
        )[0]

        for stage in (
            "ble.init.before_nimble",
            "ble.init.after_nimble",
            "ble.init.after_scan_config",
            "ble.scan.before_first_start",
            "ble.scan.after_first_start",
        ):
            self.assertIn(stage, init + scan, f"missing heap snapshot stage: {stage}")
        self.assertIn("_bleScan ? \"ready\" : \"null\"", init)
        self.assertIn("nimbleInitResult", init)
        self.assertIn('BLE-DIAG stage=ble.init result=%s', init)
        self.assertIn("firstStartResult", scan)
        self.assertIn("gmt154LogHeapSnapshot", source)
        self.assertIn("GMT154_C3_BLE_STAGE_DIAG", init + scan)

    def test_diagnostic_does_not_change_ble_initialization_or_scan_parameters(self):
        source = BLE_SOURCE.read_text(encoding="utf-8")
        init = source.split("void BleScanner::init()", 1)[1].split(
            "void BleScanner::deInit()", 1
        )[0]
        scan = source.split("bool BleScanner::scan()", 1)[1].split(
            "void BleScanner::proccesTiltBeacon", 1
        )[0]
        self.assertEqual(init.count('NimBLEDevice::init("");'), 2)
        self.assertEqual(init.count('nimbleInitResult = NimBLEDevice::init("");'), 1)
        self.assertIn(
            "nimbleInitResult = NimBLEDevice::init(\"\");",
            init,
            "diagnostic branch must capture NimBLE init status",
        )
        self.assertIn(
            "#else\n  NimBLEDevice::init(\"\");\n#endif",
            init,
            "non-diagnostic branch must retain the original NimBLE init call",
        )
        self.assertEqual(init.count("_bleScan = NimBLEDevice::getScan();"), 1)
        self.assertEqual(init.count("scan();"), 1)
        self.assertIn("setMaxResults(0)", init)
        self.assertIn("setInterval(\n      97)", init)
        self.assertIn("setWindow(37)", init)
        self.assertIn("_bleScan->start(_scanTime * 1000, false, true)", scan)
        self.assertIn("if (!_bleScan) return false;", scan)


if __name__ == "__main__":
    unittest.main(verbosity=2)
