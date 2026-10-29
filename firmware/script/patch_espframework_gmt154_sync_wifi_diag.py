"""Fail-closed instrumentation of espframework's original synchronous web scan."""
from pathlib import Path
import hashlib
import importlib.util
import os
import re
import tempfile

TARGET_ENVS = frozenset({
    'gmt154-06-sync-wifi-scan-diag',
    'gmt154-06-sync-wifi-scan-diag-ble-init-off',
})
SCRIPT_DIR = Path(__file__).resolve().parent if '__file__' in globals() else Path.cwd() / 'script'


def validate_web_overlay(source):
    markers = (
        'WSCAN-SYNC: scan begin ms=%u heap=%u largest=%u',
        'WSCAN-SYNC: scan result=%d duration_ms=%u heap=%u largest=%u',
        'WSCAN-SYNC: scan serialized_bytes=%u result=%d',
        'WSCAN-SYNC: admitted ms=%u pending=%d heap=%u largest=%u',
        'WSCAN-SYNC: reply duration_ms=%u heap=%u largest=%u',
        'WiFi.scanNetworks(false, false)',
        '#include <esp_heap_caps.h>',
    )
    if any(source.count(marker) != 1 for marker in markers):
        raise RuntimeError('Incomplete synchronous diagnostic web overlay')
    if any(marker in source for marker in (
        'WiFi.scanNetworks(true, false)',
        'WEB : Scan complete %d, %s.',
    )):
        raise RuntimeError('Unexpected web scan behavior or sensitive logging')
    calls = re.findall(r'Log\.notice\(F\("WSCAN-SYNC:[\s\S]*?;', source)
    if len(calls) != 5 or any(call.count('%u') != call.count('static_cast<uint64_t>(') for call in calls):
        raise RuntimeError('Incorrect GMT154 diagnostic unsigned log arguments')


LEGACY_WEB_OVERLAY_SHA256 = '352AADFBA4FCFA9667C74DBA8EFD0A4157F4DA1026A6BA5AB28E978B5BB573BE'


def upgrade_legacy_web_overlay(source):
    actual = hashlib.sha256(source.encode('utf-8')).hexdigest().upper()
    if actual != LEGACY_WEB_OVERLAY_SHA256:
        raise RuntimeError('Unknown or partial synchronous diagnostic web overlay')
    replacements = (
        ('Log.notice(F("WSCAN-SYNC: scan begin ms=%u heap=%u largest=%u" CR), scanStart, heapBefore, largestBefore);',
         'Log.notice(F("WSCAN-SYNC: scan begin ms=%u heap=%u largest=%u" CR),\n'
         '               static_cast<uint64_t>(scanStart), static_cast<uint64_t>(heapBefore),\n'
         '               static_cast<uint64_t>(largestBefore));'),
        ('               noNetwork, millis() - scanStart, ESP.getFreeHeap(),\n'
         '               heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));',
         '               noNetwork, static_cast<uint64_t>(millis() - scanStart),\n'
         '               static_cast<uint64_t>(ESP.getFreeHeap()),\n'
         '               static_cast<uint64_t>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));'),
        ('Log.notice(F("WSCAN-SYNC: scan serialized_bytes=%u result=%d" CR), ret, noNetwork);',
         'Log.notice(F("WSCAN-SYNC: scan serialized_bytes=%u result=%d" CR),\n'
         '               static_cast<uint64_t>(ret), noNetwork);'),
        ('             admittedAt, static_cast<int>(_wifiScanTask), ESP.getFreeHeap(),\n'
         '             heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));',
         '             static_cast<uint64_t>(admittedAt), static_cast<int>(_wifiScanTask),\n'
         '             static_cast<uint64_t>(ESP.getFreeHeap()),\n'
         '             static_cast<uint64_t>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));'),
        ('             millis() - admittedAt, ESP.getFreeHeap(),\n'
         '             heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));',
         '             static_cast<uint64_t>(millis() - admittedAt),\n'
         '             static_cast<uint64_t>(ESP.getFreeHeap()),\n'
         '             static_cast<uint64_t>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));'),
    )
    for before, after in replacements:
        if source.count(before) != 1:
            raise RuntimeError('Legacy web diagnostic overlay differs')
        source = source.replace(before, after, 1)
    validate_web_overlay(source)
    return source


def validate_wifi_overlay(source):
    markers = (
        'WSCAN-SYNC: AP create',
        'WSCAN-SYNC: strongest candidate',
        'WSCAN-SYNC: strongest selected',
        'WSCAN-SYNC: connect source=hardcoded',
        'WSCAN-SYNC: connect source=stored',
        'WSCAN-SYNC: connected ip=',
        'WSCAN-SYNC: failed configured network 1',
        'WSCAN-SYNC: connected configured network 2',
    )
    if any(source.count(marker) != 1 for marker in markers):
        raise RuntimeError('Incomplete Wi-Fi credential log overlay')
    if any(marker in source for marker in (
        'WIFI: Creating AP with %s,%s.',
        'WIFI: Found the following networks ssid:',
        'WIFI: Using ssid:',
        'WIFI: Connecting to wifi using hardcoded settings %s.',
        'WIFI: Connecting to wifi using stored settings %s.',
        'WIFI: Connected to wifi %s ip=%s.',
        'WIFI: Failed to connect to first SSID %s.',
        'WIFI: Connected to second SSID %s.',
    )):
        raise RuntimeError('Credential-bearing Wi-Fi log remains')


def patch_logging_guard(directory):
    script = SCRIPT_DIR / 'patch_espframework_gmt154_logging.py'
    spec = importlib.util.spec_from_file_location('gmt154_logging_for_sync_diag', script)
    if spec is None or spec.loader is None:
        raise RuntimeError('GMT154 logging guard patch unavailable')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.patch_directory(directory)


def patch_file(path):
    path = Path(path)
    raw = path.read_bytes()
    newline = '\r\n' if b'\r\n' in raw else '\n'
    source = raw.decode('utf-8').replace('\r\n', '\n')
    if 'WSCAN-SYNC:' in source:
        try:
            validate_web_overlay(source)
            return 'already'
        except RuntimeError:
            source = upgrade_legacy_web_overlay(source)
            updated = source.replace('\n', newline).encode('utf-8')
            temporary = None
            try:
                with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as stream:
                    temporary = Path(stream.name)
                    stream.write(updated)
                os.replace(temporary, path)
                temporary = None
            finally:
                if temporary is not None:
                    temporary.unlink(missing_ok=True)
            return 'changed'
    replacements = [
        ('#include <basewebserver.hpp>',
         '#include <basewebserver.hpp>\n#include <esp_heap_caps.h>'),
        ('    int noNetwork = WiFi.scanNetworks(false, false);',
         '    const uint32_t scanStart = millis();\n'
         '    const uint32_t heapBefore = ESP.getFreeHeap();\n'
         '    const uint32_t largestBefore = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);\n'
         '    Log.notice(F("WSCAN-SYNC: scan begin ms=%u heap=%u largest=%u" CR),\n'
         '               static_cast<uint64_t>(scanStart), static_cast<uint64_t>(heapBefore),\n'
         '               static_cast<uint64_t>(largestBefore));\n'
         '    int noNetwork = WiFi.scanNetworks(false, false);\n'
         '    Log.notice(F("WSCAN-SYNC: scan result=%d duration_ms=%u heap=%u largest=%u" CR),\n'
         '               noNetwork, static_cast<uint64_t>(millis() - scanStart),\n'
         '               static_cast<uint64_t>(ESP.getFreeHeap()),\n'
         '               static_cast<uint64_t>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));'),
        ('    Log.notice(F("WEB : Scan complete %d, %s." CR), ret, _wifiScanData.c_str());',
         '    Log.notice(F("WSCAN-SYNC: scan serialized_bytes=%u result=%d" CR),\n'
         '               static_cast<uint64_t>(ret), noNetwork);'),
        ('  _wifiScanTask = true;\n  _wifiScanData = "";',
         '  const uint32_t admittedAt = millis();\n'
         '  Log.notice(F("WSCAN-SYNC: admitted ms=%u pending=%d heap=%u largest=%u" CR),\n'
         '             static_cast<uint64_t>(admittedAt), static_cast<int>(_wifiScanTask),\n'
         '             static_cast<uint64_t>(ESP.getFreeHeap()),\n'
         '             static_cast<uint64_t>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));\n'
         '  _wifiScanTask = true;\n  _wifiScanData = "";'),
        ('  request->send(response);\n}\n\nvoid BaseWebServer::webHandleWifiScanStatus',
         '  request->send(response);\n'
         '  Log.notice(F("WSCAN-SYNC: reply duration_ms=%u heap=%u largest=%u" CR),\n'
         '             static_cast<uint64_t>(millis() - admittedAt),\n'
         '             static_cast<uint64_t>(ESP.getFreeHeap()),\n'
         '             static_cast<uint64_t>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));\n'
         '}\n\nvoid BaseWebServer::webHandleWifiScanStatus'),
    ]
    for before, after in replacements:
        if source.count(before) != 1:
            raise RuntimeError('Unknown espframework synchronous Wi-Fi scan source: ' + before[:65])
        source = source.replace(before, after, 1)
    validate_web_overlay(source)
    updated = source.replace('\n', newline).encode('utf-8')
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(updated)
        os.replace(temporary, path)
        temporary = None
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
    return 'changed'


def patch_wifi_logs(path):
    """Suppress upstream credential and SSID logs in this diagnostic env only."""
    path = Path(path)
    raw = path.read_bytes()
    newline = '\r\n' if b'\r\n' in raw else '\n'
    source = raw.decode('utf-8').replace('\r\n', '\n')
    if 'WSCAN-SYNC:' in source:
        validate_wifi_overlay(source)
        return 'already'
    replacements = [
        ('  Log.notice(F("WIFI: Creating AP with %s,%s." CR), _apSSID.c_str(),\n             _apPWD.c_str());',
         '  Log.notice(F("WSCAN-SYNC: AP create" CR));'),
        ('    Log.notice(F("WIFI: Found the following networks ssid: %s bssid: %s, rssi: "\n                 "%d channel: %d." CR),\n               WiFi.SSID(i).c_str(), WiFi.BSSIDstr(i).c_str(), WiFi.RSSI(i),\n               WiFi.channel(i));',
         '    Log.notice(F("WSCAN-SYNC: strongest candidate rssi=%d channel=%d" CR),\n               WiFi.RSSI(i), WiFi.channel(i));'),
        ('      Log.notice(F("WIFI: Using ssid: %s bssid: %s, rssi: %d channel: %d." CR),\n                 WiFi.SSID(i).c_str(), WiFi.BSSIDstr(i).c_str(), WiFi.RSSI(i),\n                 WiFi.channel(i));',
         '      Log.notice(F("WSCAN-SYNC: strongest selected rssi=%d channel=%d" CR),\n                 WiFi.RSSI(i), WiFi.channel(i));'),
        ('    Log.notice(F("WIFI: Connecting to wifi using hardcoded settings %s." CR),\n               _userSSID.c_str());',
         '    Log.notice(F("WSCAN-SYNC: connect source=hardcoded" CR));'),
        ('    Log.notice(F("WIFI: Connecting to wifi using stored settings %s." CR),\n               ssid);',
         '    Log.notice(F("WSCAN-SYNC: connect source=stored" CR));'),
        ('  Log.notice(F("WIFI: Connected to wifi %s ip=%s." CR), WiFi.SSID().c_str(),\n             getIPAddress().c_str());',
         '  Log.notice(F("WSCAN-SYNC: connected ip=%s" CR), getIPAddress().c_str());'),
        ('      Log.warning(F("WIFI: Failed to connect to first SSID %s." CR),\n                  _wifiConfig->getWifiSSID(0));',
         '      Log.warning(F("WSCAN-SYNC: failed configured network 1" CR));'),
        ('          Log.notice(F("WIFI: Connected to second SSID %s." CR),\n                     _wifiConfig->getWifiSSID(1));',
         '          Log.notice(F("WSCAN-SYNC: connected configured network 2" CR));'),
    ]
    for before, after in replacements:
        if source.count(before) != 1:
            raise RuntimeError('Unknown espframework Wi-Fi log source: ' + before[:65])
        source = source.replace(before, after, 1)
    validate_wifi_overlay(source)
    data = source.replace('\n', newline).encode('utf-8')
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
    return 'changed'


def register_hook():
    try:
        Import('env')
    except NameError:
        return
    pioenv = env.get('PIOENV')
    if pioenv not in TARGET_ENVS:
        return
    source = Path(env.subst('$PROJECT_LIBDEPS_DIR')) / pioenv / 'espframework/src/basewebserver.cpp'
    wifi_source = source.parent / 'wificonnection.cpp'
    tft_header = Path(env.subst('$PROJECT_LIBDEPS_DIR')) / pioenv / 'TFT_eSPI/Processors/TFT_eSPI_ESP32_S3.h'
    state = {'done': False}
    tft_state = {'done': False}

    def patch_before_compile(pio_env, node):
        if not state['done'] and source.exists():
            print('GMT154_SYNC_WIFI_LOG_GUARD: ' + patch_logging_guard(source.parent))
            print('GMT154_SYNC_WIFI_DIAG_PATCH: ' + patch_file(source))
            print('GMT154_SYNC_WIFI_LOG_PATCH: ' + patch_wifi_logs(wifi_source))
            state['done'] = True
        return node

    env.AddBuildMiddleware(patch_before_compile)

    def patch_tft_before_compile(pio_env, node):
        if not tft_state['done'] and 'tft_espi' in [part.casefold() for part in Path(node.srcnode().get_path()).parts]:
            script = SCRIPT_DIR / 'patch_tft_espi_gmt154.py'
            spec = importlib.util.spec_from_file_location('gmt154_tft_for_sync_diag', script)
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)
            print('GMT154_SYNC_WIFI_TFT_PATCH: ' + module.patch_header(tft_header))
            tft_state['done'] = True
        return node

    env.AddBuildMiddleware(patch_tft_before_compile)


register_hook()
