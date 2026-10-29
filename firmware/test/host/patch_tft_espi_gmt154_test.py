import importlib.util
import tempfile
import unittest
from pathlib import Path


SCRIPT_PATH = (
    Path(__file__).parents[2] / "script" / "patch_tft_espi_gmt154.py"
)
SPEC = importlib.util.spec_from_file_location("patch_tft_espi_gmt154", SCRIPT_PATH)
PATCHER = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(PATCHER)


OLD_LINE = b"      #if (TFT_MISO == -1)\r\n"
NEW_LINE = b"      #if (TFT_MISO == -1) && !defined(GMT154_06)\r\n"
HEADER_PREFIX = b"before\r\n"
HEADER_SUFFIX = b"after\r\n"


class PatchTftEspiGmt154Test(unittest.TestCase):
    def write_header(self, content: bytes) -> Path:
        path = Path(self.tempdir.name) / "TFT_eSPI_ESP32_S3.h"
        path.write_bytes(content)
        return path

    def setUp(self) -> None:
        self.tempdir = tempfile.TemporaryDirectory()

    def tearDown(self) -> None:
        self.tempdir.cleanup()

    def test_single_conversion(self) -> None:
        path = self.write_header(HEADER_PREFIX + OLD_LINE + HEADER_SUFFIX)

        result = PATCHER.apply_patch_if_gmt154("gmt154-06", path)

        self.assertEqual(result, "changed")
        self.assertEqual(path.read_bytes(), HEADER_PREFIX + NEW_LINE + HEADER_SUFFIX)

    def test_repeated_patch_is_idempotent(self) -> None:
        path = self.write_header(HEADER_PREFIX + OLD_LINE + HEADER_SUFFIX)
        PATCHER.apply_patch_if_gmt154("gmt154-06", path)
        patched = path.read_bytes()

        result = PATCHER.apply_patch_if_gmt154("gmt154-06", path)

        self.assertEqual(result, "already")
        self.assertEqual(path.read_bytes(), patched)

    def test_unknown_header_version_fails_without_modification(self) -> None:
        original = HEADER_PREFIX + b"      #if (TFT_MISO == -2)\r\n" + HEADER_SUFFIX
        path = self.write_header(original)

        with self.assertRaises(PATCHER.PatchError):
            PATCHER.apply_patch_if_gmt154("gmt154-06", path)

        self.assertEqual(path.read_bytes(), original)

    def test_multiple_anchors_fail_without_modification(self) -> None:
        original = HEADER_PREFIX + OLD_LINE + OLD_LINE + HEADER_SUFFIX
        path = self.write_header(original)

        with self.assertRaises(PATCHER.PatchError):
            PATCHER.apply_patch_if_gmt154("gmt154-06", path)

        self.assertEqual(path.read_bytes(), original)

    def test_non_gmt_environment_is_not_modified(self) -> None:
        original = HEADER_PREFIX + OLD_LINE + HEADER_SUFFIX
        path = self.write_header(original)

        result = PATCHER.apply_patch_if_gmt154("gateway32s3wave-tft-sd", path)

        self.assertEqual(result, "skipped")
        self.assertEqual(path.read_bytes(), original)

    def test_c2b_environment_uses_same_no_miso_patch(self) -> None:
        path = self.write_header(HEADER_PREFIX + OLD_LINE + HEADER_SUFFIX)

        result = PATCHER.apply_patch_if_gmt154("gmt154-06-c2b-diag", path)

        self.assertEqual(result, "changed")
        self.assertEqual(path.read_bytes(), HEADER_PREFIX + NEW_LINE + HEADER_SUFFIX)

    def test_c2c_hook_patches_active_dependency_directory(self) -> None:
        libdeps = Path(self.tempdir.name) / "libdeps"
        c2c_header = (
            libdeps
            / "gmt154-06-c2c-diag"
            / PATCHER.HEADER_RELATIVE_PATH
        )
        c2c_header.parent.mkdir(parents=True)
        c2c_header.write_bytes(HEADER_PREFIX + OLD_LINE + HEADER_SUFFIX)

        class FakeEnvironment:
            middleware = None

            def get(self, key: str) -> str:
                return "gmt154-06-c2c-diag" if key == "PIOENV" else ""

            def subst(self, value: str) -> str:
                if value != "$PROJECT_LIBDEPS_DIR":
                    raise AssertionError(value)
                return str(libdeps)

            def AddBuildMiddleware(self, callback) -> None:
                self.middleware = callback

        class FakeNode:
            def srcnode(self):
                return self

            def get_path(self) -> str:
                return str(c2c_header.parent / "TFT_eSPI.cpp")

        fake_env = FakeEnvironment()
        PATCHER.Import = lambda name: None
        PATCHER.env = fake_env
        try:
            PATCHER._register_platformio_hook()
            self.assertIsNotNone(fake_env.middleware)
            fake_env.middleware(fake_env, FakeNode())
        finally:
            del PATCHER.Import
            del PATCHER.env

        self.assertEqual(
            c2c_header.read_bytes(), HEADER_PREFIX + NEW_LINE + HEADER_SUFFIX
        )
        self.assertFalse(
            (libdeps / "gmt154-06-c2b-diag" / PATCHER.HEADER_RELATIVE_PATH).exists()
        )


if __name__ == "__main__":
    unittest.main()
