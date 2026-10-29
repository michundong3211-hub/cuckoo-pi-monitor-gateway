"""Report failed config persistence for the GMT154-06 C3 build only."""
from pathlib import Path
import os
import tempfile


TARGET_ENV = "gmt154-06-c3"
TARGET_ENVS = {
    "gmt154-06-c3-ble-profile-act3",
    "gmt154-06-c3-ble-profile-act1",
    TARGET_ENV,
    "gmt154-06-c3-mdns-off",
    "gmt154-06-c3-mdns-off-ble-init-off",
    "gmt154-06-c3-mdns-off-ble-stage-diag",
}
ORIGINAL_CONFIG_WRITE = '''  _webConfig->saveFile();
  myBatteryVoltage.read();

  doWebConfigWrite();'''
PATCHED_CONFIG_WRITE = '''  if (!_webConfig->saveFile()) {
    Log.error(F("WEB : Failed to save configuration." CR));
    AsyncJsonResponse *response = new AsyncJsonResponse(false);
    JsonObject obj = response->getRoot().as<JsonObject>();
    obj[PARAM_SUCCESS] = false;
    obj[PARAM_MESSAGE] = "Failed to save configuration";
    response->setCode(500);
    response->setLength();
    request->send(response);
    PERF_END("webserver-api-config-write");
    return;
  }
  myBatteryVoltage.read();

  doWebConfigWrite();'''


class PatchError(RuntimeError):
    """The pinned gravitymon source is unknown or only partly patched."""


def _read_source(path):
    raw = Path(path).read_bytes()
    if b"\x00" in raw:
        raise PatchError("Unexpected binary data in gravitymon source: " + str(path))
    if b"\r" in raw.replace(b"\r\n", b""):
        raise PatchError("Unsupported newline sequence in gravitymon source: " + str(path))
    newline = "\r\n" if b"\r\n" in raw else "\n"
    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError as error:
        raise PatchError("gravitymon source is not UTF-8: " + str(path)) from error
    return text.replace("\r\n", "\n"), newline


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
    """Patch only the exact pinned config-write block, preserving line endings."""
    path = Path(path)
    if not path.is_file():
        raise PatchError("Required gravitymon web_brewing.cpp is missing")

    text, newline = _read_source(path)
    original_count = text.count(ORIGINAL_CONFIG_WRITE)
    patched_count = text.count(PATCHED_CONFIG_WRITE)
    if original_count == 1 and patched_count == 0:
        state = "original"
    elif original_count == 0 and patched_count == 1:
        state = "patched"
    else:
        raise PatchError("Unknown, ambiguous, or partial config-save handler")

    if text.count("_webConfig->saveFile()") != 1:
        raise PatchError("Unexpected config-save call count; refusing to patch")
    if state == "patched":
        return "already"

    updated = text.replace(ORIGINAL_CONFIG_WRITE, PATCHED_CONFIG_WRITE, 1)
    staged = _stage_file(path, updated.replace("\n", newline).encode("utf-8"))
    try:
        os.replace(staged, path)
    finally:
        staged.unlink(missing_ok=True)
    return "changed"


def _register_platformio_hook():
    try:
        Import("env")
    except NameError:
        return

    pioenv = env.get("PIOENV")
    if pioenv not in TARGET_ENVS:
        return

    libdeps = Path(env.subst("$PROJECT_LIBDEPS_DIR"))
    source_path = libdeps / pioenv / "gravitymon" / "src" / "web_brewing.cpp"
    state = {"patched": False, "reported": False}

    if source_path.is_file():
        result = patch_source(source_path)
        state["patched"] = True
        print("GMT154_C3_CONFIG_SAVE_PATCH: " + result)
        state["reported"] = True

    def patch_before_compile(pio_env, node):
        try:
            compiled_source = Path(node.srcnode().get_path()).resolve()
            expected_source = source_path.resolve()
        except (AttributeError, OSError, TypeError):
            return node

        same_source = os.path.normcase(str(compiled_source)) == os.path.normcase(
            str(expected_source)
        )
        is_gravitymon_source = (
            compiled_source.name.casefold() == "web_brewing.cpp"
            and any(part.casefold() == "gravitymon" for part in compiled_source.parts)
        )
        if is_gravitymon_source and not same_source:
            raise PatchError(
                "gravitymon web_brewing.cpp compiled from an unexpected path: "
                + str(compiled_source)
            )
        if same_source:
            result = patch_source(compiled_source)
            state["patched"] = True
            if not state["reported"]:
                print("GMT154_C3_CONFIG_SAVE_PATCH: " + result)
                state["reported"] = True
        return node

    def verify_target_patched(target, source, env):
        if not state["patched"]:
            raise PatchError(
                "C3 build did not patch required gravitymon source: " + str(source_path)
            )

    env.AddBuildMiddleware(patch_before_compile)
    env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", verify_target_patched)


_register_platformio_hook()
