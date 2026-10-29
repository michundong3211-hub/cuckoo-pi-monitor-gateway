"""Reproducible, fail-closed GMT154 logging patch for espframework 1.2.12."""
from pathlib import Path
import os
import tempfile

SCRIPT_DIR = Path(__file__).resolve().parent if "__file__" in globals() else Path.cwd() / "script"
GUARD = SCRIPT_DIR / "patches/gmt154_logging_guard.hpp"
TARGET_ENVS = frozenset({
    "gmt154-06",
    "gmt154-06-c2b-diag",
    "gmt154-06-c2c-diag",
    "gmt154-06-wifi-scan-diag",
    "gmt154-06-sync-wifi-scan-diag",
    "gmt154-06-sync-wifi-scan-diag-ble-init-off",
})


def replace_once(text, before, after):
    if text.count(after) == 1:
        return text
    if text.count(before) != 1:
        raise RuntimeError("Unknown espframework logging source; refusing to patch")
    return text.replace(before, after, 1)


def atomic_write(path, data):
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


def patch_directory(directory):
    directory = Path(directory)
    patches = {
        "ArduinoLog.hpp": [
            ('#include "Arduino.h"', '#include "Arduino.h"\n#include "gmt154_logging_guard.hpp"'),
            ('  void printLevel(int level, bool cr, T msg, ...) {\n#ifndef ESPFWK_DISABLE_LOGGING',
             '  void printLevel(int level, bool cr, T msg, ...) {\n#ifndef ESPFWK_DISABLE_LOGGING\n'
             '#if defined(GMT154_06)\n    Gmt154LogGuard log_guard;\n#endif'),
        ],
        "serialws.cpp": [
            ('size_t SerialWebSocket::write(uint8_t c) {\n  _buf[_bufSize++] = c;',
             'size_t SerialWebSocket::write(uint8_t c) {\n#if defined(GMT154_06)\n'
             '  Gmt154LogGuard log_guard;\n'
             '  if (_bufSize >= sizeof(_buf)) flush();\n#endif\n  _buf[_bufSize++] = c;'),
            ('void SerialWebSocket::flush() {\n  if (_secondayLog)',
             'void SerialWebSocket::flush() {\n#if defined(GMT154_06)\n'
             '  Gmt154LogGuard log_guard;\n'
             '  // Detach the bounded chunk before calling a potentially reentrant sink.\n'
             '  const size_t count = _bufSize < sizeof(_buf) ? _bufSize : sizeof(_buf);\n'
             '  if (count == 0) return;\n'
             '  uint8_t chunk[sizeof(_buf)];\n'
             '  memcpy(chunk, _buf, count);\n'
             '  _bufSize = 0;\n'
             '  memset(_buf, 0, sizeof(_buf));\n'
             '  if (_secondayLog) _secondayLog->write(chunk, count);\n'
             '  if (_webSocket && _webSocket->count()) {\n'
             '    _webSocket->textAll(reinterpret_cast<const char *>(chunk), count);\n'
             '  }\n'
             '  return;\n#endif\n  if (_secondayLog)'),
        ],
    }
    pending = {}
    # Validate every anchor before modifying any dependency file.
    for filename, replacements in patches.items():
        path = directory / filename
        original = path.read_bytes()
        newline = "\r\n" if b"\r\n" in original else "\n"
        text = original.decode("utf-8").replace("\r\n", "\n")
        for before, after in replacements:
            text = replace_once(text, before, after)
        updated = text.replace("\n", newline).encode("utf-8")
        if updated != original:
            pending[path] = updated
    guard_path = directory / GUARD.name
    guard = GUARD.read_bytes()
    if guard_path.exists() and guard_path.read_bytes() != guard:
        raise RuntimeError("Existing GMT154 log guard differs; review before replacing")
    if not guard_path.exists():
        pending[guard_path] = guard
    for path, data in pending.items():
        atomic_write(path, data)
    return "changed" if pending else "already"


def register_hook():
    try:
        Import("env")
    except NameError:
        return
    pioenv = env.get("PIOENV")
    if pioenv not in TARGET_ENVS:
        return
    directory = (
        Path(env.subst("$PROJECT_LIBDEPS_DIR"))
        / f"{pioenv}/espframework/src"
    )
    state = {"done": False}

    def patch_before_compile(pio_env, node):
        if not state["done"] and (directory / "ArduinoLog.hpp").exists():
            print("GMT154_LOGGING_PATCH: " + patch_directory(directory))
            state["done"] = True
        return node

    env.AddBuildMiddleware(patch_before_compile)


register_hook()
