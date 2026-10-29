import importlib.util
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).parents[2]
PATCHER_PATH = ROOT / "script" / "patch_espframework_gmt154_c3_single_app.py"
SPEC = importlib.util.spec_from_file_location("gmt154_c3_single_app_patcher", PATCHER_PATH)
PATCHER = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(PATCHER)


ORIGINAL_SOURCE = (
    "before\n" + PATCHER.ORIGINAL_ROUTE + "\nafter\n"
)


class FakeEnvironment:
    def __init__(self, pioenv, libdeps):
        self.pioenv = pioenv
        self.libdeps = libdeps
        self.middleware = None
        self.post_action = None

    def get(self, key):
        return self.pioenv if key == "PIOENV" else ""

    def subst(self, value):
        if value != "$PROJECT_LIBDEPS_DIR":
            raise AssertionError(value)
        return str(self.libdeps)

    def AddBuildMiddleware(self, callback):
        self.middleware = callback

    def AddPostAction(self, target, callback):
        if target != "$BUILD_DIR/${PROGNAME}.elf":
            raise AssertionError(target)
        self.post_action = callback


class FakeNode:
    def __init__(self, path):
        self.path = Path(path)

    def srcnode(self):
        return self

    def get_path(self):
        return str(self.path)


class C3SingleAppPatchTest(unittest.TestCase):
    def setUp(self):
        self.tempdir = tempfile.TemporaryDirectory()
        self.root = Path(self.tempdir.name)

    def tearDown(self):
        self.tempdir.cleanup()

    def write_source(self, content=ORIGINAL_SOURCE):
        source = self.root / "basewebserver.cpp"
        source.write_bytes(content.encode("utf-8"))
        return source

    def test_original_route_changes_to_single_403_and_noop_upload_callback(self):
        source = self.write_source(ORIGINAL_SOURCE.replace("\n", "\r\n"))

        self.assertEqual(PATCHER.patch_source(source), "changed")

        patched = source.read_bytes()
        self.assertIn(b'request->send(403, "text/plain"', patched)
        self.assertIn(b"Firmware upload is disabled on GMT154-06 C3.", patched)
        self.assertIn(b"(void)data;", patched)
        self.assertNotIn(PATCHER.ORIGINAL_ROUTE.encode(), patched)
        self.assertEqual(patched.count(b'"/api/firmware", HTTP_POST'), 1)
        self.assertNotIn(b"\n", patched.replace(b"\r\n", b""))

    def test_patched_route_is_idempotent_without_changing_bytes(self):
        source = self.write_source()
        self.assertEqual(PATCHER.patch_source(source), "changed")
        patched = source.read_bytes()

        self.assertEqual(PATCHER.patch_source(source), "already")
        self.assertEqual(source.read_bytes(), patched)

    def test_unknown_or_ambiguous_route_fails_without_writing(self):
        variants = (
            ORIGINAL_SOURCE.replace("webReturnOK", "differentHandler"),
            ORIGINAL_SOURCE + "\n" + PATCHER.ORIGINAL_ROUTE + "\n",
            ORIGINAL_SOURCE.replace(
                "webHandleUploadFirmware", "webReturnOK"
            ),
        )
        for original in variants:
            with self.subTest(original=original[-60:]):
                source = self.write_source(original)
                before = source.read_bytes()
                with self.assertRaises(PATCHER.PatchError):
                    PATCHER.patch_source(source)
                self.assertEqual(source.read_bytes(), before)

    def test_only_c3_pre_script_patches_the_expected_dependency_source(self):
        libdeps = self.root / "libdeps"
        source = libdeps / PATCHER.TARGET_ENV / "espframework" / "src" / "basewebserver.cpp"
        source.parent.mkdir(parents=True)
        source.write_text(ORIGINAL_SOURCE, encoding="utf-8")
        for pioenv in ("gmt154-06", "gateway32s3wave-tft-sd"):
            env = FakeEnvironment(pioenv, libdeps)
            PATCHER.Import = lambda name: None
            PATCHER.env = env
            try:
                PATCHER._register_platformio_hook()
            finally:
                del PATCHER.Import
                del PATCHER.env
            self.assertEqual(source.read_text(encoding="utf-8"), ORIGINAL_SOURCE)
            self.assertIsNone(env.middleware)
            self.assertIsNone(env.post_action)

        env = FakeEnvironment(PATCHER.TARGET_ENV, libdeps)
        PATCHER.Import = lambda name: None
        PATCHER.env = env
        try:
            PATCHER._register_platformio_hook()
        finally:
            del PATCHER.Import
            del PATCHER.env

        self.assertIn(PATCHER.REJECTED_ROUTE, source.read_text(encoding="utf-8"))
        self.assertIsNotNone(env.middleware)
        self.assertIsNotNone(env.post_action)
        env.post_action(target=None, source=None, env=env)

    def test_missing_target_node_fails_build_at_post_action(self):
        libdeps = self.root / "missing-libdeps"
        env = FakeEnvironment(PATCHER.TARGET_ENV, libdeps)
        PATCHER.Import = lambda name: None
        PATCHER.env = env
        try:
            PATCHER._register_platformio_hook()
        finally:
            del PATCHER.Import
            del PATCHER.env
        self.assertIsNotNone(env.middleware)
        self.assertIsNotNone(env.post_action)
        env.middleware(env, FakeNode(self.root / "unrelated.cpp"))
        with self.assertRaises(PATCHER.PatchError):
            env.post_action(None, None, env)

    def test_source_materialized_during_build_is_patched_before_compile(self):
        libdeps = self.root / "late-libdeps"
        source = libdeps / PATCHER.TARGET_ENV / "espframework" / "src" / "basewebserver.cpp"
        env = FakeEnvironment(PATCHER.TARGET_ENV, libdeps)
        PATCHER.Import = lambda name: None
        PATCHER.env = env
        try:
            PATCHER._register_platformio_hook()
        finally:
            del PATCHER.Import
            del PATCHER.env

        source.parent.mkdir(parents=True)
        source.write_text(ORIGINAL_SOURCE, encoding="utf-8")
        node = FakeNode(source)
        self.assertIs(env.middleware(env, node), node)
        self.assertIn(PATCHER.REJECTED_ROUTE, source.read_text(encoding="utf-8"))
        env.post_action(target=None, source=None, env=env)

    def test_framework_source_at_unexpected_path_fails_closed(self):
        libdeps = self.root / "other-libdeps"
        env = FakeEnvironment(PATCHER.TARGET_ENV, libdeps)
        PATCHER.Import = lambda name: None
        PATCHER.env = env
        try:
            PATCHER._register_platformio_hook()
        finally:
            del PATCHER.Import
            del PATCHER.env

        wrong_source = (
            libdeps / "unexpected-env" / "espframework" / "src" / "basewebserver.cpp"
        )
        wrong_source.parent.mkdir(parents=True)
        wrong_source.write_text(ORIGINAL_SOURCE, encoding="utf-8")
        with self.assertRaises(PATCHER.PatchError):
            env.middleware(env, FakeNode(wrong_source))


if __name__ == "__main__":
    unittest.main(verbosity=2)
