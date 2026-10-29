"""Disable the espframework firmware upload route for GMT154-06 C3 only."""
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
ORIGINAL_ROUTE = '''  _server->on(
      "/api/firmware", HTTP_POST,
      std::bind(&BaseWebServer::webReturnOK, this, std::placeholders::_1),
      std::bind(&BaseWebServer::webHandleUploadFirmware, this,
                std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3, std::placeholders::_4,
                std::placeholders::_5, std::placeholders::_6));'''
REJECTED_ROUTE = '''  _server->on(
      "/api/firmware", HTTP_POST,
      [](AsyncWebServerRequest *request) {
        request->send(403, "text/plain",
                      "Firmware upload is disabled on GMT154-06 C3.");
      },
      [](AsyncWebServerRequest *request, String filename, size_t index,
         uint8_t *data, size_t len, bool final) {
        (void)request;
        (void)filename;
        (void)index;
        (void)data;
        (void)len;
        (void)final;
      });'''


class PatchError(RuntimeError):
    """The cached framework source is not in a known complete state."""


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
    """Replace the exact upload route once, preserving line endings."""
    path = Path(path)
    if not path.is_file():
        raise PatchError("Required espframework basewebserver.cpp is missing")
    original, text, newline = _read_source(path)
    original_count = text.count(ORIGINAL_ROUTE)
    patched_count = text.count(REJECTED_ROUTE)
    if original_count == 1 and patched_count == 0:
        state = "original"
    elif original_count == 0 and patched_count == 1:
        state = "patched"
    else:
        raise PatchError(
            "Unknown, ambiguous, or partial firmware route; refusing to patch"
        )

    if text.count('"/api/firmware", HTTP_POST') != 1:
        raise PatchError("Unexpected /api/firmware POST route count; refusing to patch")
    if state == "patched":
        return "already"

    updated = text.replace(ORIGINAL_ROUTE, REJECTED_ROUTE, 1)
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
    source_path = libdeps / pioenv / "espframework" / "src" / "basewebserver.cpp"
    state = {"patched": False, "reported": False}

    # Libdeps normally exists before extra_scripts run. If it does, patch now,
    # including an unchanged restored cache. If dependency materialization is
    # deferred, the compile middleware below must find the exact target and the
    # post-action will fail if it never does.
    if source_path.is_file():
        result = patch_source(source_path)
        state["patched"] = True
        print("GMT154_C3_SINGLE_APP_PATCH: " + result)
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
        is_framework_source = (
            compiled_source.name.casefold() == "basewebserver.cpp"
            and any(part.casefold() == "espframework" for part in compiled_source.parts)
        )
        if is_framework_source and not same_source:
            raise PatchError(
                "espframework basewebserver.cpp compiled from an unexpected path: "
                + str(compiled_source)
            )
        if same_source:
            result = patch_source(compiled_source)
            state["patched"] = True
            if not state["reported"]:
                print("GMT154_C3_SINGLE_APP_PATCH: " + result)
                state["reported"] = True
        return node

    def verify_target_patched(target, source, env):
        if not state["patched"]:
            raise PatchError(
                "C3 build did not patch the required espframework source: "
                + str(source_path)
            )

    env.AddBuildMiddleware(patch_before_compile)
    env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", verify_target_patched)


_register_platformio_hook()
