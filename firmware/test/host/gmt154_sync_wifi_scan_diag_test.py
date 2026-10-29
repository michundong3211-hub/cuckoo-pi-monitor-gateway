import importlib.util
import re
import shutil
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).parents[2]
SCRIPT = ROOT / 'script/patch_espframework_gmt154_sync_wifi_diag.py'
SPEC = importlib.util.spec_from_file_location('sync_wifi_diag', SCRIPT)
PATCH = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PATCH)


class SyncWifiScanDiagnosticTest(unittest.TestCase):
    def test_logger_u_contract_requires_64_bit_diagnostic_arguments(self):
        formatter = (ROOT / '.pio/libdeps/gmt154-06-sync-wifi-scan-diag/espframework/src/ArduinoLog.cpp').read_text(encoding='utf-8')
        self.assertRegex(formatter, r"format == 'u'[\s\S]{0,100}va_arg\(\*args, uint64_t\)")
        source = (ROOT / '.pio/libdeps/gateway32-unit/espframework/src/basewebserver.cpp').read_text(encoding='utf-8')
        with tempfile.TemporaryDirectory() as temp:
            target = Path(temp) / 'basewebserver.cpp'
            target.write_text(source, encoding='utf-8')
            PATCH.patch_file(target)
            updated = target.read_text(encoding='utf-8')
            calls = re.findall(r'Log\.notice\(F\("WSCAN-SYNC:[\s\S]*?;', updated)
            self.assertEqual(len(calls), 5)
            for call in calls:
                self.assertEqual(call.count('%u'), call.count('static_cast<uint64_t>('), call)

    def test_partial_web_overlay_is_rejected_without_write(self):
        source = (ROOT / '.pio/libdeps/gateway32-unit/espframework/src/basewebserver.cpp').read_text(encoding='utf-8')
        with tempfile.TemporaryDirectory() as temp:
            target = Path(temp) / 'basewebserver.cpp'
            target.write_text(source, encoding='utf-8')
            PATCH.patch_file(target)
            partial = target.read_text(encoding='utf-8').replace('WSCAN-SYNC: reply', 'WEB: reply missing', 1)
            target.write_text(partial, encoding='utf-8')
            with self.assertRaises(RuntimeError):
                PATCH.patch_file(target)
            self.assertEqual(target.read_text(encoding='utf-8'), partial)

    def test_cached_overlay_has_u64_diagnostics(self):
        cached = ROOT / '.pio/libdeps/gmt154-06-sync-wifi-scan-diag/espframework/src/basewebserver.cpp'
        with tempfile.TemporaryDirectory() as temp:
            target = Path(temp) / 'basewebserver.cpp'
            shutil.copyfile(cached, target)
            self.assertIn(PATCH.patch_file(target), ('changed', 'already'))
            updated = target.read_text(encoding='utf-8')
            calls = re.findall(r'Log\.notice\(F\("WSCAN-SYNC:[\s\S]*?;', updated)
            self.assertEqual(len(calls), 5)
            for call in calls:
                self.assertEqual(call.count('%u'), call.count('static_cast<uint64_t>('), call)

    def test_partial_wifi_log_overlay_is_rejected_without_write(self):
        source = (ROOT / '.pio/libdeps/gateway32-unit/espframework/src/wificonnection.cpp').read_text(encoding='utf-8')
        with tempfile.TemporaryDirectory() as temp:
            target = Path(temp) / 'wificonnection.cpp'
            target.write_text(source, encoding='utf-8')
            PATCH.patch_wifi_logs(target)
            partial = target.read_text(encoding='utf-8').replace(
                'Log.notice(F("WSCAN-SYNC: connect source=stored" CR));',
                'Log.notice(F("WIFI: Connecting to wifi using stored settings %s." CR), ssid);', 1)
            target.write_text(partial, encoding='utf-8')
            with self.assertRaises(RuntimeError):
                PATCH.patch_wifi_logs(target)
            self.assertEqual(target.read_text(encoding='utf-8'), partial)

    def test_existing_gmt154_log_guard_applies_to_diagnostic_cache(self):
        upstream = ROOT / '.pio/libdeps/gateway32-unit/espframework/src'
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            for name in ('ArduinoLog.hpp', 'serialws.cpp'):
                shutil.copyfile(upstream / name, directory / name)
            self.assertEqual(PATCH.patch_logging_guard(directory), 'changed')
            self.assertIn('Gmt154LogGuard log_guard;', (directory / 'ArduinoLog.hpp').read_text(encoding='utf-8'))
            self.assertIn('Gmt154LogGuard log_guard;', (directory / 'serialws.cpp').read_text(encoding='utf-8'))
            self.assertEqual(PATCH.patch_logging_guard(directory), 'already')

    def test_preserves_sync_scan_and_records_admission_completion(self):
        source = (ROOT / '.pio/libdeps/gateway32-unit/espframework/src/basewebserver.cpp').read_text(encoding='utf-8')
        with tempfile.TemporaryDirectory() as temp:
            target = Path(temp) / 'basewebserver.cpp'
            target.write_text(source, encoding='utf-8')
            self.assertEqual(PATCH.patch_file(target), 'changed')
            updated = target.read_text(encoding='utf-8')
            self.assertIn('WiFi.scanNetworks(false, false)', updated)
            self.assertNotIn('WiFi.scanNetworks(true, false)', updated)
            self.assertIn('WSCAN-SYNC: admitted', updated)
            self.assertIn('WSCAN-SYNC: scan result=', updated)
            self.assertIn('WSCAN-SYNC: reply', updated)
            self.assertNotIn('_wifiScanData.c_str()', updated)
            self.assertEqual(PATCH.patch_file(target), 'already')

    def test_unknown_scan_source_fails_closed(self):
        with tempfile.TemporaryDirectory() as temp:
            target = Path(temp) / 'basewebserver.cpp'
            target.write_text('unknown', encoding='utf-8')
            with self.assertRaises(RuntimeError):
                PATCH.patch_file(target)
            self.assertEqual(target.read_text(encoding='utf-8'), 'unknown')

    def test_wifi_connection_diagnostic_suppresses_credential_logs(self):
        source = (ROOT / '.pio/libdeps/gateway32-unit/espframework/src/wificonnection.cpp').read_text(encoding='utf-8')
        with tempfile.TemporaryDirectory() as temp:
            target = Path(temp) / 'wificonnection.cpp'
            target.write_text(source, encoding='utf-8')
            self.assertEqual(PATCH.patch_wifi_logs(target), 'changed')
            updated = target.read_text(encoding='utf-8')
            self.assertNotIn('Log.notice(F("WIFI: Creating AP with %s,%s.', updated)
            self.assertNotIn('Log.notice(F("WIFI: Found the following networks ssid:', updated)
            self.assertNotIn('Log.notice(F("WIFI: Connecting to wifi using stored settings %s.', updated)
            self.assertEqual(PATCH.patch_wifi_logs(target), 'already')


if __name__ == '__main__':
    unittest.main()
