"""Fail-closed GMT154-06 reset and Wi-Fi log patch for espframework."""
from pathlib import Path
import os
import tempfile


TARGET_ENV = "gmt154-06"
SCRIPT_DIR = Path(__file__).resolve().parent if "__file__" in globals() else Path.cwd() / "script"

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
UTILS_INCLUDE = "#include <utils.hpp>"
ESP_SYSTEM_INCLUDE = "#include <esp_system.h>"


class PatchError(RuntimeError):
    """The cached dependency does not match a complete supported patch state."""


def _decode_source(path):
    raw = Path(path).read_bytes()
    if b"\x00" in raw:
        raise PatchError("Unexpected binary data in espframework source: " + str(path))
    if b"\r" in raw.replace(b"\r\n", b""):
        raise PatchError("Unsupported newline sequence in espframework source: " + str(path))
    newline = "\r\n" if b"\r\n" in raw else "\n"
    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError as error:
        raise PatchError("espframework source is not UTF-8: " + str(path)) from error
    return raw, text.replace("\r\n", "\n"), newline


def _classify(text, original, patched, label):
    original_count = text.count(original)
    patched_count = text.count(patched)
    if original_count == 1 and patched_count == 0:
        return "original"
    if patched_count == 1 and original_count == 0:
        return "patched"
    raise PatchError("Unknown or partial " + label + " source; refusing to patch")


def _stage_file(path, data):
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        return temporary
    except Exception:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
        raise


def _atomic_write_many(pending, originals):
    staged = {}
    replaced = []
    try:
        for path, data in pending.items():
            staged[path] = _stage_file(path, data)
        for path, temporary in staged.items():
            os.replace(temporary, path)
            replaced.append(path)
    except Exception as error:
        rollback_errors = []
        for path in reversed(replaced):
            try:
                rollback = _stage_file(path, originals[path])
                os.replace(rollback, path)
            except Exception as rollback_error:
                rollback_errors.append(str(rollback_error))
        detail = ""
        if rollback_errors:
            detail = "; rollback errors: " + "; ".join(rollback_errors)
        raise PatchError("Could not atomically apply GMT154 runtime patch" + detail) from error
    finally:
        for temporary in staged.values():
            temporary.unlink(missing_ok=True)


def patch_directory(directory):
    """Patch both framework sources only from the complete original state."""
    directory = Path(directory)
    utils_path = directory / "utils.cpp"
    wifi_path = directory / "wificonnection.cpp"
    if not utils_path.is_file() or not wifi_path.is_file():
        raise PatchError("Required espframework runtime sources are missing")

    originals = {}
    texts = {}
    newlines = {}
    for path in (utils_path, wifi_path):
        raw, text, newline = _decode_source(path)
        originals[path] = raw
        texts[path] = text
        newlines[path] = newline

    utils = texts[utils_path]
    wifi = texts[wifi_path]
    if utils.count(UTILS_INCLUDE) != 1:
        raise PatchError("Unknown utils.cpp include structure; refusing to patch")
    reset_state = _classify(utils, ORIGINAL_RESET, PATCHED_RESET, "forcedReset")
    esp_include_count = utils.count(ESP_SYSTEM_INCLUDE)
    if reset_state == "original" and esp_include_count != 0:
        raise PatchError("Unexpected esp_system.h state with original forcedReset")
    if reset_state == "patched" and esp_include_count != 1:
        raise PatchError("Patched forcedReset requires one esp_system.h include")

    wifi_state = _classify(wifi, ORIGINAL_WIFI_LOG, PATCHED_WIFI_LOG, "stored-SSID log")
    if reset_state != wifi_state:
        raise PatchError("Mixed original/patch state across espframework sources; refusing to patch")
    if reset_state == "patched":
        return "already"

    updated_utils = utils.replace(ORIGINAL_RESET, PATCHED_RESET, 1)
    updated_utils = updated_utils.replace(
        UTILS_INCLUDE,
        UTILS_INCLUDE + "\n" + ESP_SYSTEM_INCLUDE,
        1,
    )
    updated_wifi = wifi.replace(ORIGINAL_WIFI_LOG, PATCHED_WIFI_LOG, 1)
    pending = {
        utils_path: updated_utils.replace("\n", newlines[utils_path]).encode("utf-8"),
        wifi_path: updated_wifi.replace("\n", newlines[wifi_path]).encode("utf-8"),
    }
    _atomic_write_many(pending, originals)
    return "changed"


def _register_platformio_hook():
    try:
        Import("env")
    except NameError:
        return

    pioenv = env.get("PIOENV")
    if pioenv != TARGET_ENV:
        return

    libdeps = Path(env.subst("$PROJECT_LIBDEPS_DIR"))
    directory = libdeps / TARGET_ENV / "espframework" / "src"
    state = {"done": False}

    def patch_before_compile(pio_env, node):
        if state["done"]:
            return node
        try:
            source_path = Path(node.srcnode().get_path())
        except (AttributeError, OSError, TypeError):
            return node
        if "espframework" not in [part.casefold() for part in source_path.parts]:
            return node
        result = patch_directory(directory)
        print("GMT154_RUNTIME_PATCH: " + result)
        state["done"] = True
        return node

    env.AddBuildMiddleware(patch_before_compile)


_register_platformio_hook()
