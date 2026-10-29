"""Disable mDNS registration in both espframework backends for C3 diagnostics only."""
from pathlib import Path
import os
import tempfile


TARGET_ENV = "gmt154-06-c3-mdns-off"
TARGET_ENVS = frozenset(
    {
        TARGET_ENV,
        "gmt154-06-c3-mdns-off-ble-init-off",
        "gmt154-06-c3-mdns-off-ble-stage-diag",
    }
)
SOURCE_NAMES = ("basewebserver.cpp", "basewebserver2.cpp")
GRAVITYMON_SOURCE_NAME = "web_brewing.cpp"
ORIGINAL_MDNS_BLOCK = '''  MDNS.begin(_webConfig->getMDNS());
  MDNS.addService("http", "tcp", 80);
  MDNS.addServiceTxt("http", "tcp", "ver", CFG_APPVER);
  MDNS.addServiceTxt("http", "tcp", "app", CFG_APPNAME);
  MDNS.addServiceTxt("http", "tcp", "id", _webConfig->getID());'''
PATCHED_MDNS_BLOCK = '''#if !defined(GMT154_C3_MDNS_OFF_DIAG)
  MDNS.begin(_webConfig->getMDNS());
  MDNS.addService("http", "tcp", 80);
  MDNS.addServiceTxt("http", "tcp", "ver", CFG_APPVER);
  MDNS.addServiceTxt("http", "tcp", "app", CFG_APPNAME);
  MDNS.addServiceTxt("http", "tcp", "id", _webConfig->getID());
#endif'''
ORIGINAL_GRAVITYMON_MDNS_BLOCK = '''  MDNS.addService(serviceName, "tcp", 80);
  MDNS.addServiceTxt(serviceName, "tcp", "ver", CFG_APPVER);
  MDNS.addServiceTxt(serviceName, "tcp", "app", CFG_APPNAME);
  MDNS.addServiceTxt(serviceName, "tcp", "id", _webConfig->getID());'''
PATCHED_GRAVITYMON_MDNS_BLOCK = '''#if !defined(GMT154_C3_MDNS_OFF_DIAG)
  MDNS.addService(serviceName, "tcp", 80);
  MDNS.addServiceTxt(serviceName, "tcp", "ver", CFG_APPVER);
  MDNS.addServiceTxt(serviceName, "tcp", "app", CFG_APPNAME);
  MDNS.addServiceTxt(serviceName, "tcp", "id", _webConfig->getID());
#endif'''


class PatchError(RuntimeError):
    """The pinned dependency source is missing, unknown, or partially patched."""


def _read_source(path):
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


def _classify(text, path):
    original_count = text.count(ORIGINAL_MDNS_BLOCK)
    patched_count = text.count(PATCHED_MDNS_BLOCK)
    # The patched form intentionally contains the original call sequence
    # inside its compile-time guard, so both counts are one when patched.
    if original_count == 1 and patched_count == 1:
        state = "patched"
    elif original_count == 1 and patched_count == 0:
        state = "original"
    else:
        raise PatchError("Unknown or partial mDNS source block: " + str(path))

    total_calls = sum(
        text.count(token)
        for token in (
            "MDNS.begin(_webConfig->getMDNS());",
            'MDNS.addService("http", "tcp", 80);',
            'MDNS.addServiceTxt("http", "tcp", "ver", CFG_APPVER);',
            'MDNS.addServiceTxt("http", "tcp", "app", CFG_APPNAME);',
            'MDNS.addServiceTxt("http", "tcp", "id", _webConfig->getID());',
        )
    )
    if total_calls != 5:
        raise PatchError("Unexpected mDNS registration call count: " + str(path))
    return state


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


def patch_source(path):
    """Patch one exact backend block, preserving its line endings."""
    path = Path(path)
    if path.name not in SOURCE_NAMES or not path.is_file():
        raise PatchError("Required espframework backend source is missing: " + str(path))
    raw, text, newline = _read_source(path)
    state = _classify(text, path)
    if state == "patched":
        return "already"
    updated = text.replace(ORIGINAL_MDNS_BLOCK, PATCHED_MDNS_BLOCK, 1)
    staged = _stage_file(path, updated.replace("\n", newline).encode("utf-8"))
    try:
        os.replace(staged, path)
    finally:
        staged.unlink(missing_ok=True)
    return "changed"


def patch_directory(source_dir):
    """Validate both backend files before patching either one."""
    source_dir = Path(source_dir)
    sources = [source_dir / name for name in SOURCE_NAMES]
    loaded = []
    for path in sources:
        if not path.is_file():
            raise PatchError("Required espframework backend source is missing: " + str(path))
        raw, text, newline = _read_source(path)
        state = _classify(text, path)
        loaded.append((path, raw, text, newline, state))

    pending = {}
    for path, raw, text, newline, state in loaded:
        if state == "original":
            updated = text.replace(ORIGINAL_MDNS_BLOCK, PATCHED_MDNS_BLOCK, 1)
            pending[path] = (raw, updated.replace("\n", newline).encode("utf-8"))

    staged = {}
    replaced = []
    try:
        for path, (_, data) in pending.items():
            staged[path] = _stage_file(path, data)
        for path, temporary in staged.items():
            os.replace(temporary, path)
            replaced.append(path)
    except Exception as error:
        for path in reversed(replaced):
            rollback = _stage_file(path, pending[path][0])
            try:
                os.replace(rollback, path)
            finally:
                rollback.unlink(missing_ok=True)
        raise PatchError("Could not apply mDNS patch to both backends") from error
    finally:
        for temporary in staged.values():
            temporary.unlink(missing_ok=True)

    if not pending:
        return "already"
    return "changed"


def patch_gravitymon_source(path):
    """Disable gravitymon's own HTTP mDNS announcements exactly once."""
    path = Path(path)
    if path.name != GRAVITYMON_SOURCE_NAME or not path.is_file():
        raise PatchError("Required gravitymon web backend source is missing: " + str(path))
    raw, text, newline = _read_source(path)
    original_count = text.count(ORIGINAL_GRAVITYMON_MDNS_BLOCK)
    patched_count = text.count(PATCHED_GRAVITYMON_MDNS_BLOCK)
    if original_count == 1 and patched_count == 0:
        state = "original"
    elif original_count == 1 and patched_count == 1:
        state = "patched"
    else:
        raise PatchError("Unknown or partial gravitymon mDNS block: " + str(path))

    tokens = (
        'MDNS.addService(serviceName, "tcp", 80);',
        'MDNS.addServiceTxt(serviceName, "tcp", "ver", CFG_APPVER);',
        'MDNS.addServiceTxt(serviceName, "tcp", "app", CFG_APPNAME);',
        'MDNS.addServiceTxt(serviceName, "tcp", "id", _webConfig->getID());',
    )
    if any(text.count(token) != 1 for token in tokens):
        raise PatchError("Unexpected gravitymon mDNS registration call count: " + str(path))
    if state == "patched":
        return "already"

    updated = text.replace(
        ORIGINAL_GRAVITYMON_MDNS_BLOCK, PATCHED_GRAVITYMON_MDNS_BLOCK, 1
    )
    staged = _stage_file(path, updated.replace("\n", newline).encode("utf-8"))
    try:
        os.replace(staged, path)
    finally:
        staged.unlink(missing_ok=True)
    return "changed"


def patch_environment(environment_dir):
    """Patch both framework backends and gravitymon in one environment cache."""
    environment_dir = Path(environment_dir)
    framework_result = patch_directory(environment_dir / "espframework" / "src")
    gravitymon_result = patch_gravitymon_source(
        environment_dir / "gravitymon" / "src" / GRAVITYMON_SOURCE_NAME
    )
    return (
        "changed"
        if "changed" in (framework_result, gravitymon_result)
        else "already"
    )


def _register_platformio_hook():
    try:
        Import("env")
    except NameError:
        return

    pioenv = env.get("PIOENV")
    if pioenv not in TARGET_ENVS:
        return

    libdeps = Path(env.subst("$PROJECT_LIBDEPS_DIR"))
    environment_dir = libdeps / pioenv
    source_dir = environment_dir / "espframework" / "src"
    gravitymon_path = environment_dir / "gravitymon" / "src" / GRAVITYMON_SOURCE_NAME
    patched = {name: False for name in (*SOURCE_NAMES, GRAVITYMON_SOURCE_NAME)}
    reported = False

    if all((source_dir / name).is_file() for name in SOURCE_NAMES) and gravitymon_path.is_file():
        result = patch_environment(environment_dir)
        patched.update({name: True for name in patched})
        print("GMT154_C3_MDNS_OFF_PATCH: " + result)
        reported = True

    def patch_before_compile(pio_env, node):
        try:
            compiled_source = Path(node.srcnode().get_path()).resolve()
        except (AttributeError, OSError, TypeError):
            return node

        name = compiled_source.name.casefold()
        matching_name = next((item for item in SOURCE_NAMES if item.casefold() == name), None)
        parts = {part.casefold() for part in compiled_source.parts}
        is_framework_source = "espframework" in parts
        is_gravitymon_source = name == GRAVITYMON_SOURCE_NAME.casefold() and "gravitymon" in parts
        if matching_name is not None and is_framework_source:
            expected_source = (source_dir / matching_name).resolve()
        elif is_gravitymon_source:
            expected_source = gravitymon_path.resolve()
        else:
            expected_source = None
        if expected_source is not None:
            if os.path.normcase(str(compiled_source)) != os.path.normcase(
                str(expected_source)
            ):
                raise PatchError(
                    "diagnostic web backend compiled from an unexpected path: "
                    + str(compiled_source)
                )
            result = patch_environment(environment_dir)
            patched.update({item: True for item in patched})
            nonlocal reported
            if not reported:
                print("GMT154_C3_MDNS_OFF_PATCH: " + result)
                reported = True
        return node

    def verify_target_patched(target, source, env):
        missing = [name for name, value in patched.items() if not value]
        if missing:
            raise PatchError(
                "C3 mDNS diagnostic did not patch all web registration sources: "
                + ", ".join(missing)
            )

    env.AddBuildMiddleware(patch_before_compile)
    env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", verify_target_patched)


_register_platformio_hook()
