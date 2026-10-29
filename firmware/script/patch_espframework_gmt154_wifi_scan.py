"""Fail-closed GMT154-only asynchronous Wi-Fi scan patch for espframework."""
from pathlib import Path
import importlib.util
import os
import tempfile

SCRIPT_DIR = Path(__file__).resolve().parent if '__file__' in globals() else Path.cwd() / 'script'
TARGET_ENVS = frozenset({'gmt154-06', 'gmt154-06-c2b-diag', 'gmt154-06-c2c-diag', 'gmt154-06-wifi-scan-diag'})

ORIGINAL_LOOP = '''  if (_wifiScanTask) {
    JsonDocument doc;

    doc[PARAM_STATUS] = false;
    doc[PARAM_SUCCESS] = true;
    doc[PARAM_MESSAGE] = "";

    JsonArray networks = doc[PARAM_NETWORKS].to<JsonArray>();

    Log.notice(F("WEB : Scanning for wifi networks." CR));
    int noNetwork = WiFi.scanNetworks(false, false);

    for (int i = 0; i < noNetwork; i++) {
      networks[i][PARAM_SSID] = WiFi.SSID(i);
      networks[i][PARAM_RSSI] = WiFi.RSSI(i);
      networks[i][PARAM_CHANNEL] = WiFi.channel(i);
#ifdef ESP8266
      networks[i][PARAM_ENCRYPTION] = WiFi.encryptionType(i);
#else
      networks[i][PARAM_ENCRYPTION] = WiFi.encryptionType(i);
#endif
    }

    unsigned int ret = serializeJson(doc, _wifiScanData);
    Log.notice(F("WEB : Scan complete %d, %s." CR), ret, _wifiScanData.c_str());
    _wifiScanTask = false;
  }'''


def validate_diagnostic_scan_control_block(source):
    contract_path = SCRIPT_DIR / 'patches/gmt154_wifi_scan_diag_contract.py'
    spec = importlib.util.spec_from_file_location('gmt154_wifi_scan_diag_contract', contract_path)
    if spec is None or spec.loader is None:
        raise RuntimeError('GMT154 Wi-Fi scan diagnostic contract is unavailable')
    contract = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(contract)
    contract.validate_scan_loop(source, (contract.DIAGNOSTIC_SCAN_LOOP_SHA256,))


def replace_once(source, old, new):
    if source.count(new) == 1:
        return source
    if source.count(old) != 1:
        raise RuntimeError('Unknown espframework Wi-Fi scan source; refusing to patch')
    return source.replace(old, new, 1)


def patch_directory(directory):
    directory = Path(directory)
    cpp = directory / 'basewebserver.cpp'
    header = directory / 'basewebserver.hpp'
    originals = {path: path.read_bytes() for path in (cpp, header)}
    texts = {path: raw.decode('utf-8').replace('\r\n', '\n') for path, raw in originals.items()}

    new_loop = '''  xSemaphoreTake(_wifiScanMutex, portMAX_DELAY);
  const bool pending = _wifiScanTask;
  const bool started = _wifiScanStarted;
  xSemaphoreGive(_wifiScanMutex);
  if (pending) {
    if (!started) {
      Log.notice(F("WEB : Scanning for wifi networks." CR));
      const int result = WiFi.scanNetworks(true, false);
      xSemaphoreTake(_wifiScanMutex, portMAX_DELAY);
      _wifiScanStarted = (result == WIFI_SCAN_RUNNING);
      xSemaphoreGive(_wifiScanMutex);
      if (result == WIFI_SCAN_RUNNING) return;
      // A completed zero-network scan is also a valid terminal result.
      if (result < 0) {
        xSemaphoreTake(_wifiScanMutex, portMAX_DELAY);
        _wifiScanData = "{\\"status\\":false,\\"success\\":false,\\"message\\":\\"Wifi scanning failed\\",\\"networks\\":[]}";
        _wifiScanTask = false;
        xSemaphoreGive(_wifiScanMutex);
        WiFi.scanDelete();
        return;
      }
    }
    const int noNetwork = WiFi.scanComplete();
    if (noNetwork == WIFI_SCAN_RUNNING) return;
    JsonDocument doc;
    doc[PARAM_STATUS] = false;
    doc[PARAM_SUCCESS] = noNetwork >= 0;
    doc[PARAM_MESSAGE] = noNetwork >= 0 ? "" : "Wifi scanning failed";
    JsonArray networks = doc[PARAM_NETWORKS].to<JsonArray>();
    for (int i = 0; i < noNetwork; i++) {
      networks[i][PARAM_SSID] = WiFi.SSID(i);
      networks[i][PARAM_RSSI] = WiFi.RSSI(i);
      networks[i][PARAM_CHANNEL] = WiFi.channel(i);
      networks[i][PARAM_ENCRYPTION] = WiFi.encryptionType(i);
    }
    String data;
    serializeJson(doc, data);
    WiFi.scanDelete();
    xSemaphoreTake(_wifiScanMutex, portMAX_DELAY);
    _wifiScanData = data;
    _wifiScanStarted = false;
    _wifiScanTask = false;
    xSemaphoreGive(_wifiScanMutex);
    Log.notice(F("WEB : Scan complete %d, %s." CR), noNetwork, data.c_str());
  }'''
    final_loop = new_loop.replace(
        '    WiFi.scanDelete();\n    xSemaphoreTake(_wifiScanMutex, portMAX_DELAY);',
        '    if (noNetwork < 0) esp_wifi_scan_stop();\n    WiFi.scanDelete();\n    xSemaphoreTake(_wifiScanMutex, portMAX_DELAY);')
    loop_end = '\n}\n\nvoid BaseWebServer::webHandleUploadFirmware'
    starts = ('  if (_wifiScanTask) {\n    JsonDocument doc;',
              '  xSemaphoreTake(_wifiScanMutex, portMAX_DELAY);\n  const bool pending')
    found = [(marker, texts[cpp].index(marker)) for marker in starts if texts[cpp].count(marker) == 1]
    if len(found) != 1 or texts[cpp].count(loop_end) != 1:
        raise RuntimeError('Unknown espframework Wi-Fi scan loop; refusing to patch')
    start = found[0][1]
    end = texts[cpp].index(loop_end, start)
    current_loop = texts[cpp][start:end]
    if current_loop not in (ORIGINAL_LOOP, new_loop, final_loop):
        raise RuntimeError('Unknown espframework Wi-Fi scan loop; refusing to patch')
    texts[cpp] = texts[cpp][:start] + final_loop + texts[cpp][end:]
    if texts[cpp].count('esp_wifi_scan_stop()') != 1 or 'WiFi.scanNetworks(false, false)' in texts[cpp]:
        raise RuntimeError('Unexpected Wi-Fi scan code outside patch block')
    texts[cpp] = replace_once(texts[cpp], '#include <basewebserver.hpp>',
                              '#include <basewebserver.hpp>\n#include <esp_wifi.h>')

    texts[header] = replace_once(texts[header], '#include <freertos/FreeRTOS.h>', '#include <freertos/FreeRTOS.h>\n#include <freertos/semphr.h>')
    dynamic_mutex = '  volatile bool _wifiScanTask = false;\n  bool _wifiScanStarted = false;\n  SemaphoreHandle_t _wifiScanMutex = xSemaphoreCreateMutex();'
    static_mutex = '  volatile bool _wifiScanTask = false;\n  bool _wifiScanStarted = false;\n  StaticSemaphore_t _wifiScanMutexStorage;\n  SemaphoreHandle_t _wifiScanMutex = xSemaphoreCreateMutexStatic(&_wifiScanMutexStorage);'
    if dynamic_mutex in texts[header]:
        texts[header] = replace_once(texts[header], dynamic_mutex, static_mutex)
    else:
        texts[header] = replace_once(texts[header], '  volatile bool _wifiScanTask = false;', static_mutex)
    texts[cpp] = replace_once(texts[cpp], '  _wifiScanTask = true;\n  _wifiScanData = "";',
                              '  xSemaphoreTake(_wifiScanMutex, portMAX_DELAY);\n  if (!_wifiScanTask) {\n    _wifiScanTask = true;\n    _wifiScanData = "";\n  }\n  xSemaphoreGive(_wifiScanMutex);')
    old_status = '''  if (_wifiScanTask || !_wifiScanData.length()) {'''
    new_status = '''  xSemaphoreTake(_wifiScanMutex, portMAX_DELAY);
  const bool scanTask = _wifiScanTask;
  const String scanData = _wifiScanData;
  xSemaphoreGive(_wifiScanMutex);
  if (scanTask || !scanData.length()) {'''
    texts[cpp] = replace_once(texts[cpp], old_status, new_status)
    texts[cpp] = replace_once(texts[cpp], 'obj[PARAM_STATUS] = static_cast<bool>(_wifiScanTask);', 'obj[PARAM_STATUS] = scanTask;')
    texts[cpp] = replace_once(texts[cpp], '        _wifiScanTask ? "Wifi scanning running" : "No scanning running";',
                              '        scanTask ? "Wifi scanning running" : "No scanning running";')
    texts[cpp] = replace_once(texts[cpp], 'request->send(200, "application/json", _wifiScanData);',
                              'request->send(200, "application/json", scanData);')

    pending = {}
    for path, source in texts.items():
        newline = '\r\n' if b'\r\n' in originals[path] else '\n'
        data = source.replace('\n', newline).encode('utf-8')
        if data != originals[path]:
            pending[path] = data
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
    pioenv = env.get('PIOENV')
    if pioenv not in TARGET_ENVS:
        return
    directory = Path(env.subst('$PROJECT_LIBDEPS_DIR')) / pioenv / 'espframework' / 'src'
    state = {'done': False}

    def patch_before_compile(pio_env, node):
        if not state['done'] and (directory / 'basewebserver.cpp').exists():
            if pioenv == 'gmt154-06-wifi-scan-diag' and b'WSCAN: web start result=' in (directory / 'basewebserver.cpp').read_bytes():
                source = (directory / 'basewebserver.cpp').read_text(encoding='utf-8')
                if (source.count('WiFi.scanComplete()') != 1 or
                        source.count('WiFi.scanDelete()') != 2 or
                        source.count('esp_wifi_scan_stop()') != 1 or
                        source.count('if (result == WIFI_SCAN_RUNNING) return;') != 1 or
                        source.count('if (noNetwork == WIFI_SCAN_RUNNING) return;') != 1 or
                        'WiFi.scanNetworks(false, false)' in source or
                        'xSemaphoreTake(_wifiScanMutex, portMAX_DELAY)' not in source):
                    raise RuntimeError('Unknown GMT154 diagnostic Wi-Fi scan overlay')
                validate_diagnostic_scan_control_block(source)
                result = 'already (diagnostic overlay)'
            else:
                result = patch_directory(directory)
            print('GMT154_WIFI_SCAN_PATCH: ' + result)
            state['done'] = True
        return node

    env.AddBuildMiddleware(patch_before_compile)


register_hook()
