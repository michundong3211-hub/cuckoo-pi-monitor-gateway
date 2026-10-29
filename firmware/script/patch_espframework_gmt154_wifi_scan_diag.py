"""Fail-closed, idempotent diagnostic overlay for the GMT154 espframework scan."""
from pathlib import Path
import hashlib
import importlib.util
import os
import tempfile

SCRIPT_DIR = Path(__file__).resolve().parent if '__file__' in globals() else Path.cwd() / 'script'
HEADER = SCRIPT_DIR / 'patches/gmt154_wifi_scan_diag.hpp'
TARGET = 'gmt154-06-wifi-scan-diag'
LEGACY_HEADER_SHA256 = '57F0A2AFFB8A65B46CD594163A1529DAFDA614597427A2B7AAECA50847EDEA37'
SCAN_CONTRACT_PATH = SCRIPT_DIR / 'patches/gmt154_wifi_scan_diag_contract.py'


def replace_once(text, before, after):
    if text.count(after) == 1:
        return text
    if text.count(before) != 1:
        raise RuntimeError('Unknown espframework Wi-Fi scan diagnostic source at ' + repr(before[:90]))
    return text.replace(before, after, 1)


def load_scan_contract():
    spec = importlib.util.spec_from_file_location('gmt154_wifi_scan_diag_contract', SCAN_CONTRACT_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError('GMT154 Wi-Fi scan diagnostic contract is unavailable')
    contract = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(contract)
    return contract


def validate_scan_control_flow(source):
    contract = load_scan_contract()
    contract.validate_scan_loop(source, (
        contract.BASE_SCAN_LOOP_SHA256,
        contract.DIAGNOSTIC_SCAN_LOOP_SHA256,
    ))


def patch_directory(directory):
    directory = Path(directory)
    changes = {
        'basewebserver.cpp': [
            ('#include <esp_wifi.h>', '#include <esp_wifi.h>\n#include <gmt154_wifi_scan_diag.hpp>'),
            ('void BaseWebServer::loop() {\n  if (!_server) return;', 'void BaseWebServer::loop() {\n  if (!_server) return;\n#ifdef GMT154_WIFI_SCAN_DIAG\n  gmt154_wifi_diag::drain();\n#endif'),
            ('      const int result = WiFi.scanNetworks(true, false);',
             '      const int result = WiFi.scanNetworks(true, false);\n#ifdef GMT154_WIFI_SCAN_DIAG\n'
             '      Log.notice(F("WSCAN: web start result=%d" CR), result);\n#endif'),
            ('    const int noNetwork = WiFi.scanComplete();',
             '    const int noNetwork = WiFi.scanComplete();\n#ifdef GMT154_WIFI_SCAN_DIAG\n'
             '    Log.notice(F("WSCAN: web poll result=%d" CR), noNetwork);\n#endif'),
            ('        WiFi.scanDelete();\n        return;',
             '        WiFi.scanDelete();\n#ifdef GMT154_WIFI_SCAN_DIAG\n'
             '        Log.notice(F("WSCAN: web delete start-error" CR));\n#endif\n        return;'),
            ('    if (noNetwork < 0) esp_wifi_scan_stop();\n    WiFi.scanDelete();',
             '    if (noNetwork < 0) esp_wifi_scan_stop();\n#ifdef GMT154_WIFI_SCAN_DIAG\n'
             '    if (noNetwork < 0) Log.notice(F("WSCAN: web stop result=%d" CR), noNetwork);\n#endif\n'
             '    WiFi.scanDelete();\n#ifdef GMT154_WIFI_SCAN_DIAG\n'
             '    Log.notice(F("WSCAN: web delete result=%d" CR), noNetwork);\n#endif'),
            ('    Log.notice(F("WEB : Scan complete %d, %s." CR), noNetwork, data.c_str());',
             '#ifdef GMT154_WIFI_SCAN_DIAG\n'
             '    Log.notice(F("WSCAN: web complete result=%d" CR), noNetwork);\n#else\n'
             '    Log.notice(F("WEB : Scan complete %d, %s." CR), noNetwork, data.c_str());\n#endif'),
            ('  Log.notice(F("WEB : webServer callback for /api/wifi/scan." CR));',
             '  Log.notice(F("WEB : webServer callback for /api/wifi/scan." CR));\n#ifdef GMT154_WIFI_SCAN_DIAG\n'
             '  Log.notice(F("WSCAN: web request ms=%u" CR), millis());\n#endif'),
            ('  Log.notice(F("WEB : webServer callback for /api/wifi/scan/status." CR));',
             '  Log.notice(F("WEB : webServer callback for /api/wifi/scan/status." CR));\n#ifdef GMT154_WIFI_SCAN_DIAG\n'
             '  Log.notice(F("WSCAN: web status request ms=%u" CR), millis());\n#endif'),
            ('  if (scanTask || !scanData.length()) {',
             '#ifdef GMT154_WIFI_SCAN_DIAG\n'
             '  Log.notice(F("WSCAN: web status pending=%d bytes=%u" CR), scanTask, static_cast<unsigned>(scanData.length()));\n#endif\n'
             '  if (scanTask || !scanData.length()) {'),
        ],
        'wificonnection.cpp': [
            ('#include <WiFi.h>', '#include <WiFi.h>\n#include <gmt154_wifi_scan_diag.hpp>'),
            ('  Log.notice(F("WIFI: Creating AP with %s,%s." CR), _apSSID.c_str(),\n'
             '             _apPWD.c_str());',
             '#ifdef GMT154_WIFI_SCAN_DIAG\n'
             '  Log.notice(F("WIFI: Creating access point." CR));\n'
             '#else\n'
             '  Log.notice(F("WIFI: Creating AP with %s,%s." CR), _apSSID.c_str(),\n'
             '             _apPWD.c_str());\n#endif'),
            ('const uint8_t *WifiConnection::findStrongestAP(String &ssid) {',
             'const uint8_t *WifiConnection::findStrongestAP(String &ssid) {\n#ifdef GMT154_WIFI_SCAN_DIAG\n'
             '  Log.notice(F("WSCAN: strongest entry ms=%u" CR), millis());\n#endif'),
            ('  int noNetwork = WiFi.scanNetworks(false, false, false, 300, 0, ssid.c_str());',
             '  int noNetwork = WiFi.scanNetworks(false, false, false, 300, 0, ssid.c_str());\n'
             '#ifdef GMT154_WIFI_SCAN_DIAG\n'
             '  Log.notice(F("WSCAN: strongest scan result=%d" CR), noNetwork);\n#endif'),
            ('    Log.notice(F("WIFI: Found the following networks ssid: %s bssid: %s, rssi: "',
             '    #ifndef GMT154_WIFI_SCAN_DIAG\n'
             '    Log.notice(F("WIFI: Found the following networks ssid: %s bssid: %s, rssi: "'),
            ('               WiFi.channel(i));\n  }',
             '               WiFi.channel(i));\n    #endif\n  }'),
            ('      Log.notice(F("WIFI: Using ssid: %s bssid: %s, rssi: %d channel: %d." CR),',
             '      #ifndef GMT154_WIFI_SCAN_DIAG\n'
             '      Log.notice(F("WIFI: Using ssid: %s bssid: %s, rssi: %d channel: %d." CR),'),
            ('      return WiFi.BSSID(i);',
             '      #ifdef GMT154_WIFI_SCAN_DIAG\n'
             '      Log.notice(F("WSCAN: strongest exit found=1" CR));\n      #endif\n'
             '      return WiFi.BSSID(i);'),
            ('                 WiFi.channel(i));\n      #ifdef GMT154_WIFI_SCAN_DIAG',
             '                 WiFi.channel(i));\n      #endif\n      #ifdef GMT154_WIFI_SCAN_DIAG'),
            ('  return NULL;\n}',
             '  #ifdef GMT154_WIFI_SCAN_DIAG\n'
             '  Log.notice(F("WSCAN: strongest exit found=0" CR));\n  #endif\n'
             '  return NULL;\n}'),
            ('    Log.notice(F("WIFI: Connecting to wifi using hardcoded settings %s." CR),\n'
             '               _userSSID.c_str());',
             '#ifdef GMT154_WIFI_SCAN_DIAG\n'
             '    Log.notice(F("WSCAN: reconnect source=hardcoded" CR));\n#else\n'
             '    Log.notice(F("WIFI: Connecting to wifi using hardcoded settings %s." CR),\n'
             '               _userSSID.c_str());\n#endif'),
            ('    Log.notice(F("WIFI: Connecting to wifi using stored settings %s." CR),\n'
             '               ssid);',
             '#ifdef GMT154_WIFI_SCAN_DIAG\n'
             '    Log.notice(F("WSCAN: reconnect source=stored" CR));\n#else\n'
             '    Log.notice(F("WIFI: Connecting to wifi using stored settings %s." CR),\n'
             '               ssid);\n#endif'),
            ('  Log.notice(F("WIFI: Connected to wifi %s ip=%s." CR), WiFi.SSID().c_str(),\n'
             '             getIPAddress().c_str());',
             '#ifdef GMT154_WIFI_SCAN_DIAG\n'
             '  Log.notice(F("WIFI: Connected to wifi ip=%s." CR), getIPAddress().c_str());\n'
             '#else\n'
             '  Log.notice(F("WIFI: Connected to wifi %s ip=%s." CR), WiFi.SSID().c_str(),\n'
             '             getIPAddress().c_str());\n#endif'),
            ('      Log.warning(F("WIFI: Failed to connect to first SSID %s." CR),\n'
             '                  _wifiConfig->getWifiSSID(0));',
             '#ifdef GMT154_WIFI_SCAN_DIAG\n'
             '      Log.warning(F("WIFI: Failed to connect to configured network 1." CR));\n'
             '#else\n'
             '      Log.warning(F("WIFI: Failed to connect to first SSID %s." CR),\n'
             '                  _wifiConfig->getWifiSSID(0));\n#endif'),
            ('          Log.notice(F("WIFI: Connected to second SSID %s." CR),\n'
             '                     _wifiConfig->getWifiSSID(1));',
             '#ifdef GMT154_WIFI_SCAN_DIAG\n'
             '          Log.notice(F("WIFI: Connected to configured network 2." CR));\n'
             '#else\n'
             '          Log.notice(F("WIFI: Connected to second SSID %s." CR),\n'
             '                     _wifiConfig->getWifiSSID(1));\n#endif'),
        ],
        'improvWiFi/ImprovWiFi.cpp': [
            ('#include <improvWiFi/ImprovWiFi.h>',
             '#include <improvWiFi/ImprovWiFi.h>\n#include <gmt154_wifi_scan_diag.hpp>'),
            ('void ImprovWiFi::getAvailableWifiNetworks() {',
             'void ImprovWiFi::getAvailableWifiNetworks() {\n#ifdef GMT154_WIFI_SCAN_DIAG\n'
             '  Log.notice(F("WSCAN: improv entry ms=%u" CR), millis());\n#endif'),
            ('  int networkNum = WiFi.scanNetworks();',
             '  int networkNum = WiFi.scanNetworks();\n#ifdef GMT154_WIFI_SCAN_DIAG\n'
             '  Log.notice(F("WSCAN: improv scan result=%d" CR), networkNum);\n#endif'),
            ('  send_response(data);\n}\n\nvoid ImprovWiFi::onErrorCallback',
             '  send_response(data);\n#ifdef GMT154_WIFI_SCAN_DIAG\n'
             '  Log.notice(F("WSCAN: improv exit result=%d" CR), networkNum);\n#endif\n}\n\nvoid ImprovWiFi::onErrorCallback'),
        ],
    }
    pending = {}
    for name, replacements in changes.items():
        path = directory / name
        raw = path.read_bytes()
        newline = '\r\n' if b'\r\n' in raw else '\n'
        text = raw.decode('utf-8').replace('\r\n', '\n')
        if name == 'basewebserver.cpp':
            validate_scan_control_flow(text)
        for before, after in replacements:
            text = replace_once(text, before, after)
        updated = text.replace('\n', newline).encode('utf-8')
        if updated != raw:
            pending[path] = updated
    target = directory / HEADER.name
    content = HEADER.read_bytes()
    if target.exists():
        existing_header = target.read_bytes()
        if existing_header != content:
            canonical_header = existing_header.replace(b'\r\n', b'\n')
            digest = hashlib.sha256(canonical_header).hexdigest().upper()
            if digest != LEGACY_HEADER_SHA256:
                raise RuntimeError('Existing GMT154 Wi-Fi diagnostic header differs')
            pending[target] = content
    else:
        pending[target] = content
    for path, data in pending.items():
        temporary = None
        try:
            with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as stream:
                temporary = Path(stream.name)
                stream.write(data)
            os.replace(temporary, path)
            temporary = None
        finally:
            if temporary is not None:
                temporary.unlink(missing_ok=True)
    return 'changed' if pending else 'already'


def register_hook():
    try:
        Import('env')
    except NameError:
        return
    if env.get('PIOENV') != TARGET:
        return
    directory = Path(env.subst('$PROJECT_LIBDEPS_DIR')) / TARGET / 'espframework' / 'src'
    tft_path = Path(env.subst('$PROJECT_LIBDEPS_DIR')) / TARGET / 'TFT_eSPI' / 'Processors' / 'TFT_eSPI_ESP32_S3.h'
    state = {'done': False}
    tft_state = {'done': False}

    def patch_before_compile(pio_env, node):
        if not state['done'] and (directory / 'basewebserver.cpp').exists():
            print('GMT154_WIFI_SCAN_DIAG_PATCH: ' + patch_directory(directory))
            state['done'] = True
        return node

    env.AddBuildMiddleware(patch_before_compile)

    def patch_tft_before_compile(pio_env, node):
        if not tft_state['done'] and 'tft_espi' in [part.casefold() for part in Path(node.srcnode().get_path()).parts]:
            spec = importlib.util.spec_from_file_location('gmt154_tft_patch_for_wifi_diag', SCRIPT_DIR / 'patch_tft_espi_gmt154.py')
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)
            print('GMT154_WIFI_DIAG_TFT_PATCH: ' + module.patch_header(tft_path))
            tft_state['done'] = True
        return node

    env.AddBuildMiddleware(patch_tft_before_compile)


register_hook()
