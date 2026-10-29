"""C3-only NimBLE connection budget; preserve all BLE roles and scanners.

The complete normalized upstream header is pinned, including its position of SDK
and legacy macro loading. Any unreviewed drift fails before a file is replaced.
"""
import hashlib
import os
from pathlib import Path
import tempfile

TARGET_ENVS = {
    'gmt154-06-c3-ble-profile-act3', 'gmt154-06-c3-ble-profile-act1',
    'gmt154-06-c3', 'gmt154-06-c3-mdns-off',
    'gmt154-06-c3-mdns-off-ble-init-off',
    'gmt154-06-c3-mdns-off-ble-stage-diag',
}
UPSTREAM_SHA256 = '551e23986ea611c0c7e1567e2c80e72e7ac15af62239a5ded2d6049f71e10a72'
ANCHOR = '#include "nimconfig_rename.h"\n'
PATCH_BLOCK = '''
// GMT154_C3_CONNECTION_BUDGET_BEGIN
#if defined(GMT154_06_C3) && GMT154_06_C3
// sdkconfig.h defines the new name unconditionally; cover its legacy alias too.
#undef CONFIG_BT_NIMBLE_MAX_CONNECTIONS
#undef CONFIG_NIMBLE_MAX_CONNECTIONS
#define CONFIG_BT_NIMBLE_MAX_CONNECTIONS 1
#define CONFIG_NIMBLE_MAX_CONNECTIONS 1
#if CONFIG_BT_NIMBLE_ROLE_CENTRAL != 1 || CONFIG_BT_NIMBLE_ROLE_PERIPHERAL != 1 || CONFIG_BT_NIMBLE_ROLE_BROADCASTER != 1 || CONFIG_BT_NIMBLE_ROLE_OBSERVER != 1
#error "C3 connection-budget candidate must preserve all four BLE roles"
#endif
#endif
// GMT154_C3_CONNECTION_BUDGET_END
'''


class PatchError(RuntimeError):
    """Unrecognized or incomplete dependency state; never silently continue."""


def patch_source(path):
    path = Path(path)
    if not path.is_file():
        raise PatchError('Required NimBLE nimconfig.h is missing: ' + str(path))
    raw = path.read_bytes()
    if b'\x00' in raw or b'\r' in raw.replace(b'\r\n', b''):
        raise PatchError('Unsupported NimBLE header encoding or newline sequence')
    try:
        text = raw.decode('utf-8').replace('\r\n', '\n')
    except UnicodeDecodeError as error:
        raise PatchError('NimBLE header is not UTF-8') from error
    newline = '\r\n' if b'\r\n' in raw else '\n'
    original = text
    state = 'changed'
    if text.count(PATCH_BLOCK) == 1:
        # Validation includes exact insertion position, not merely block presence.
        original = text.replace(ANCHOR + PATCH_BLOCK, ANCHOR, 1)
        state = 'already'
    if hashlib.sha256(original.encode('utf-8')).hexdigest() != UPSTREAM_SHA256:
        raise PatchError('Unknown, partial, duplicate, or drifted NimBLE config; refusing to patch')
    if original.count(ANCHOR) != 1:
        raise PatchError('NimBLE config include layout is not the reviewed version')
    if state == 'already':
        return state
    updated = original.replace(ANCHOR, ANCHOR + PATCH_BLOCK, 1)
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
    return state


def _register_platformio_hook():
    try:
        Import('env')
    except NameError:
        return
    pioenv = env.get('PIOENV')
    if pioenv not in TARGET_ENVS:
        return
    source_root = Path(env.subst('$PROJECT_LIBDEPS_DIR')) / pioenv / 'NimBLE-Arduino' / 'src'
    header = source_root / 'nimconfig.h'
    state = {'patched': False}

    def apply():
        result = patch_source(header)
        if not state['patched']:
            print('GMT154_C3_CONNECTION_BUDGET_PATCH: ' + result)
        state['patched'] = True

    if header.exists():
        apply()

    def before_compile(pio_env, node):
        # A project source may include NimBLE before its own library is built.
        if header.is_file():
            apply()
        try:
            source = Path(node.srcnode().get_path()).resolve()
        except (AttributeError, OSError, TypeError):
            return node
        if any(part.casefold() == 'nimble-arduino' for part in source.parts):
            expected = source_root.resolve()
            if not source.is_relative_to(expected):
                raise PatchError('NimBLE compiled from unexpected dependency path: ' + str(source))
            apply()
        return node

    def verify(target, source, env):
        if not state['patched']:
            raise PatchError('C3 build did not apply NimBLE connection-budget patch')
        # Revalidate even a previously patched dependency before accepting ELF.
        patch_source(header)

    env.AddBuildMiddleware(before_compile)
    env.AddPostAction('$BUILD_DIR/${PROGNAME}.elf', verify)


_register_platformio_hook()
