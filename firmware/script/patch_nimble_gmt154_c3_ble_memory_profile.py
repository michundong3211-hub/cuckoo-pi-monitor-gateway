"""Pinned C3-only BLE activity comparison and actual initialization phase ledger.

Only the two explicit profile environments are patched. Full normalized upstream
source and every exact owned block/insertion position are validated fail-closed.
"""
import hashlib
import os
from pathlib import Path
import tempfile

TARGET_ENVS = {'gmt154-06-c3-ble-profile-act3', 'gmt154-06-c3-ble-profile-act1'}
UPSTREAM_SHA256 = '74016a2e6df8f66a37702dea1d14c78ffe650a129b427b7b63c759af32d84768'
PATCHES = (
    (
        '''#include "NimBLEDevice.h"
''',
        '''// GMT154_C3_BLE_PROFILE_BEGIN declarations
#include "NimBLEDevice.h"
#if defined(GMT154_06_C3) && GMT154_06_C3 && defined(GMT154_C3_BLE_MEMORY_PROFILE) && GMT154_C3_BLE_MEMORY_PROFILE
extern void gmt154LogHeapSnapshot(const char* stage);
extern void gmt154LogBleInitResult(const char* phase, int result);
#if !defined(GMT154_C3_BLE_CONTROLLER_ACTIVITIES) || (GMT154_C3_BLE_CONTROLLER_ACTIVITIES != 1 && GMT154_C3_BLE_CONTROLLER_ACTIVITIES != 3)
#error "C3 BLE memory profile requires reviewed activity count 1 or 3"
#endif
#endif
// GMT154_C3_BLE_PROFILE_END declarations
'''
    ),
    (
        '''        bt_cfg.ble_max_act = MYNEWT_VAL(BLE_MAX_CONNECTIONS) + MYNEWT_VAL(BLE_ROLE_BROADCASTER) + MYNEWT_VAL(BLE_ROLE_OBSERVER);
''',
        '''// GMT154_C3_BLE_PROFILE_BEGIN activities
        bt_cfg.ble_max_act = MYNEWT_VAL(BLE_MAX_CONNECTIONS) + MYNEWT_VAL(BLE_ROLE_BROADCASTER) + MYNEWT_VAL(BLE_ROLE_OBSERVER);
#if defined(GMT154_06_C3) && GMT154_06_C3 && defined(GMT154_C3_BLE_MEMORY_PROFILE) && GMT154_C3_BLE_MEMORY_PROFILE
        bt_cfg.ble_max_act = GMT154_C3_BLE_CONTROLLER_ACTIVITIES;
#if GMT154_C3_BLE_CONTROLLER_ACTIVITIES == 1
        ::gmt154LogHeapSnapshot("ble.profile.activities.1");
#else
        ::gmt154LogHeapSnapshot("ble.profile.activities.3");
#endif
#endif
// GMT154_C3_BLE_PROFILE_END activities
'''
    ),
    (
        '''        err = esp_bt_controller_init(&bt_cfg);
''',
        '''// GMT154_C3_BLE_PROFILE_BEGIN controller_init
#if defined(GMT154_06_C3) && GMT154_06_C3 && defined(GMT154_C3_BLE_MEMORY_PROFILE) && GMT154_C3_BLE_MEMORY_PROFILE
        ::gmt154LogHeapSnapshot("ble.profile.controller_init.before");
#endif
        err = esp_bt_controller_init(&bt_cfg);
#if defined(GMT154_06_C3) && GMT154_06_C3 && defined(GMT154_C3_BLE_MEMORY_PROFILE) && GMT154_C3_BLE_MEMORY_PROFILE
        ::gmt154LogHeapSnapshot("ble.profile.controller_init.after");
        ::gmt154LogBleInitResult("controller_init", static_cast<int>(err));
#endif
// GMT154_C3_BLE_PROFILE_END controller_init
'''
    ),
    (
        '''        err = esp_bt_controller_enable(ESP_BT_MODE_BLE);
''',
        '''// GMT154_C3_BLE_PROFILE_BEGIN controller_enable
#if defined(GMT154_06_C3) && GMT154_06_C3 && defined(GMT154_C3_BLE_MEMORY_PROFILE) && GMT154_C3_BLE_MEMORY_PROFILE
        ::gmt154LogHeapSnapshot("ble.profile.controller_enable.before");
#endif
        err = esp_bt_controller_enable(ESP_BT_MODE_BLE);
#if defined(GMT154_06_C3) && GMT154_06_C3 && defined(GMT154_C3_BLE_MEMORY_PROFILE) && GMT154_C3_BLE_MEMORY_PROFILE
        ::gmt154LogHeapSnapshot("ble.profile.controller_enable.after");
        ::gmt154LogBleInitResult("controller_enable", static_cast<int>(err));
#endif
// GMT154_C3_BLE_PROFILE_END controller_enable
'''
    ),
    (
        '''#   if CONFIG_BT_NIMBLE_LEGACY_VHCI_ENABLE
        err = esp_nimble_hci_init();
        if (err != ESP_OK) {
            NIMBLE_LOGE(LOG_TAG, "esp_nimble_hci_init() failed; err=%d", err);
            return false;
        }
#   endif
''',
        '''// GMT154_C3_BLE_PROFILE_BEGIN legacy_hci
#   if CONFIG_BT_NIMBLE_LEGACY_VHCI_ENABLE
#if defined(GMT154_06_C3) && GMT154_06_C3 && defined(GMT154_C3_BLE_MEMORY_PROFILE) && GMT154_C3_BLE_MEMORY_PROFILE
        ::gmt154LogHeapSnapshot("ble.profile.legacy_hci.before");
#endif
        err = esp_nimble_hci_init();
#if defined(GMT154_06_C3) && GMT154_06_C3 && defined(GMT154_C3_BLE_MEMORY_PROFILE) && GMT154_C3_BLE_MEMORY_PROFILE
        ::gmt154LogHeapSnapshot("ble.profile.legacy_hci.after");
        ::gmt154LogBleInitResult("legacy_hci", static_cast<int>(err));
#endif
        if (err != ESP_OK) {
            NIMBLE_LOGE(LOG_TAG, "esp_nimble_hci_init() failed; err=%d", err);
            return false;
        }
#   else
#if defined(GMT154_06_C3) && GMT154_06_C3 && defined(GMT154_C3_BLE_MEMORY_PROFILE) && GMT154_C3_BLE_MEMORY_PROFILE
        ::gmt154LogHeapSnapshot("ble.profile.legacy_hci.skip");
#endif
#   endif
// GMT154_C3_BLE_PROFILE_END legacy_hci
'''
    ),
    (
        '''        nimble_port_init();
''',
        '''// GMT154_C3_BLE_PROFILE_BEGIN host_port_init
#if defined(GMT154_06_C3) && GMT154_06_C3 && defined(GMT154_C3_BLE_MEMORY_PROFILE) && GMT154_C3_BLE_MEMORY_PROFILE
        ::gmt154LogHeapSnapshot("ble.profile.host_port_init.before");
        const esp_err_t gmt154PortInitResult = nimble_port_init();
        ::gmt154LogHeapSnapshot("ble.profile.host_port_init.after");
        ::gmt154LogBleInitResult("host_port_init", static_cast<int>(gmt154PortInitResult));
        if (gmt154PortInitResult != ESP_OK) {
            return false;
        }
#else
        nimble_port_init();
#endif
// GMT154_C3_BLE_PROFILE_END host_port_init
'''
    ),
    (
        '''        nimble_port_freertos_init(NimBLEDevice::host_task);
''',
        '''// GMT154_C3_BLE_PROFILE_BEGIN host_thread_call
#if defined(GMT154_06_C3) && GMT154_06_C3 && defined(GMT154_C3_BLE_MEMORY_PROFILE) && GMT154_C3_BLE_MEMORY_PROFILE
        ::gmt154LogHeapSnapshot("ble.profile.host_thread_call.before");
#endif
        nimble_port_freertos_init(NimBLEDevice::host_task);
#if defined(GMT154_06_C3) && GMT154_06_C3 && defined(GMT154_C3_BLE_MEMORY_PROFILE) && GMT154_C3_BLE_MEMORY_PROFILE
        ::gmt154LogHeapSnapshot("ble.profile.host_thread_call.after");
#endif
// GMT154_C3_BLE_PROFILE_END host_thread_call
'''
    ),
)

class PatchError(RuntimeError):
    """Unknown dependency content or source path; refuse to build it."""


def patch_source(path):
    path = Path(path)
    if not path.is_file():
        raise PatchError('Required NimBLEDevice.cpp missing: ' + str(path))
    raw = path.read_bytes()
    if b'\x00' in raw or b'\r' in raw.replace(b'\r\n', b''):
        raise PatchError('Unsupported NimBLE source encoding/newlines')
    try:
        text = raw.decode('utf-8').replace('\r\n', '\n')
    except UnicodeDecodeError as error:
        raise PatchError('NimBLE source is not UTF-8') from error
    newline = '\r\n' if b'\r\n' in raw else '\n'
    present = [text.count(new) for old, new in PATCHES]
    if any(present) and present != [1] * len(PATCHES):
        raise PatchError('Partial or duplicate BLE profile blocks')
    original = text
    if any(present):
        for old, new in PATCHES:
            original = original.replace(new, old, 1)
    if hashlib.sha256(original.encode('utf-8')).hexdigest() != UPSTREAM_SHA256:
        raise PatchError('Unknown, partial, duplicate, moved, or drifted NimBLE source')
    if any(original.count(old) != 1 for old, new in PATCHES):
        raise PatchError('Reviewed NimBLE layout anchors are missing or duplicated')
    if any(present):
        return 'already'
    updated = original
    for old, new in PATCHES:
        updated = updated.replace(old, new, 1)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(updated.replace('\n', newline).encode('utf-8'))
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
    return 'changed'


def _register_platformio_hook():
    try:
        Import('env')
    except NameError:
        return
    pioenv = env.get('PIOENV')
    if pioenv not in TARGET_ENVS:
        return
    source_root = Path(env.subst('$PROJECT_LIBDEPS_DIR')) / pioenv / 'NimBLE-Arduino' / 'src'
    path = source_root / 'NimBLEDevice.cpp'
    state = {'patched': False}
    def apply():
        result = patch_source(path)
        if not state['patched']:
            print('GMT154_C3_BLE_MEMORY_PROFILE_PATCH: ' + result)
        state['patched'] = True
    if path.is_file():
        apply()
    def before_compile(pio_env, node):
        # A main source may include dependency headers before library compilation.
        if path.is_file():
            apply()
        try:
            source = Path(node.srcnode().get_path()).resolve()
        except (AttributeError, OSError, TypeError):
            return node
        if any(part.casefold() == 'nimble-arduino' for part in source.parts):
            if not source.is_relative_to(source_root.resolve()):
                raise PatchError('Unexpected NimBLE dependency path: ' + str(source))
            apply()
        return node
    def verify(target, source, env):
        if not state['patched']:
            raise PatchError('C3 BLE profile build never applied its source patch')
        patch_source(path)
    env.AddBuildMiddleware(before_compile)
    env.AddPostAction('$BUILD_DIR/${PROGNAME}.elf', verify)


_register_platformio_hook()
