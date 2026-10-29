import builtins
import importlib.util
import shutil
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch


ROOT = Path(__file__).parents[2]
NORMAL = "gmt154-06-c3"
DIAG = "gmt154-06-c3-mdns-off"
BLE_DIAG = "gmt154-06-c3-mdns-off-ble-init-off"
BLE_STAGE_DIAG = "gmt154-06-c3-mdns-off-ble-stage-diag"
DIAG_ENVS = (DIAG, BLE_DIAG, BLE_STAGE_DIAG)
PINNED = ROOT / ".pio" / "libdeps" / NORMAL
PATCHERS = {
    "tft": ROOT / "script" / "patch_tft_espi_gmt154_c3.py",
    "single_app": ROOT / "script" / "patch_espframework_gmt154_c3_single_app.py",
    "config_save": ROOT / "script" / "patch_gravitymon_gmt154_c3_config_save.py",
    "mdns": ROOT / "script" / "patch_espframework_gmt154_c3_mdns_off.py",
}


def load(name):
    spec = importlib.util.spec_from_file_location("diag_hook_" + name, PATCHERS[name])
    if spec is None or spec.loader is None:
        raise AssertionError("cannot load patcher " + name)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class FakeEnvironment:
    def __init__(self, pioenv, libdeps):
        self.values = {"PIOENV": pioenv}
        self.libdeps = str(libdeps)
        self.middleware = []
        self.actions = []

    def get(self, key):
        return self.values.get(key)

    def subst(self, value):
        return self.libdeps

    def AddBuildMiddleware(self, callback):
        self.middleware.append(callback)

    def AddPostAction(self, target, callback):
        self.actions.append((target, callback))


def copy_dependency_fixture(destination):
    shutil.copytree(PINNED / "TFT_eSPI", destination / "TFT_eSPI")
    shutil.copytree(PINNED / "espframework", destination / "espframework")
    shutil.copytree(PINNED / "gravitymon", destination / "gravitymon")


def import_patcher_with_env(name, pioenv, libdeps_root):
    fake = FakeEnvironment(pioenv, libdeps_root)
    module = load(name)
    def platformio_import(symbol):
        if symbol != "env":
            raise AssertionError("unexpected PlatformIO import: " + symbol)
        module.env = fake
    with patch.object(builtins, "Import", platformio_import, create=True):
        module._register_platformio_hook()
    return module, fake


class Gmt154C3DiagnosticDependencyHooksTest(unittest.TestCase):
    def test_diagnostic_runs_all_four_hooks_on_its_own_dependencies(self):
        for path in PATCHERS.values():
            self.assertTrue(path.is_file(), "missing patcher: " + str(path))
        self.assertTrue(PINNED.is_dir(), "pinned normal C3 dependency fixture is missing")

        with tempfile.TemporaryDirectory() as temporary:
            libdeps = Path(temporary)
            normal_dir = libdeps / NORMAL
            copy_dependency_fixture(normal_dir)
            diag_dirs = {}
            for diag_env in DIAG_ENVS:
                diag_dir = libdeps / diag_env
                copy_dependency_fixture(diag_dir)
                diag_dirs[diag_env] = diag_dir

            # Each diagnostic target must receive all inherited repairs and the
            # full mDNS registration gate in its own PIOENV dependency directory.
            tft = load("tft")
            for diag_env, diag_dir in diag_dirs.items():
                self.assertNotEqual(tft.apply_patch_for_env(diag_env, libdeps), "skipped")
                _, single_app_env = import_patcher_with_env("single_app", diag_env, libdeps)
                _, config_env = import_patcher_with_env("config_save", diag_env, libdeps)
                _, mdns_env = import_patcher_with_env("mdns", diag_env, libdeps)
                self.assertTrue(single_app_env.middleware and single_app_env.actions)
                self.assertTrue(config_env.middleware and config_env.actions)
                self.assertTrue(mdns_env.middleware and mdns_env.actions)

                diag_framework = diag_dir / "espframework" / "src" / "basewebserver.cpp"
                diag_config = diag_dir / "gravitymon" / "src" / "web_brewing.cpp"
                diag_tft = diag_dir / "TFT_eSPI" / "Processors" / "TFT_eSPI_ESP32_C3.h"
                self.assertIn("Firmware upload is disabled on GMT154-06 C3.", diag_framework.read_text())
                self.assertIn("Failed to save configuration", diag_config.read_text())
                self.assertIn("SPI_REG_PORT", diag_tft.read_text())
                for name in ("basewebserver.cpp", "basewebserver2.cpp"):
                    content = (diag_dir / "espframework" / "src" / name).read_text()
                    self.assertIn("#if !defined(GMT154_C3_MDNS_OFF_DIAG)", content)
                patcher = load("mdns")
                self.assertIn(patcher.PATCHED_GRAVITYMON_MDNS_BLOCK, diag_config.read_text())

            # Normal C3 stays byte-for-byte as copied from its established cache;
            # its inherited hooks stay idempotent and the mDNS-only hook is gated.
            before = {
                path.relative_to(normal_dir): path.read_bytes()
                for path in normal_dir.rglob("*") if path.is_file()
            }
            self.assertNotEqual(tft.apply_patch_for_env(NORMAL, libdeps), "skipped")
            _, normal_single = import_patcher_with_env("single_app", NORMAL, libdeps)
            _, normal_config = import_patcher_with_env("config_save", NORMAL, libdeps)
            _, normal_mdns = import_patcher_with_env("mdns", NORMAL, libdeps)
            self.assertTrue(normal_single.middleware and normal_single.actions)
            self.assertTrue(normal_config.middleware and normal_config.actions)
            self.assertFalse(normal_mdns.middleware or normal_mdns.actions)
            after = {
                path.relative_to(normal_dir): path.read_bytes()
                for path in normal_dir.rglob("*") if path.is_file()
            }
            self.assertEqual(before, after)
            self.assertNotIn(
                "#if !defined(GMT154_C3_MDNS_OFF_DIAG)",
                (normal_dir / "espframework" / "src" / "basewebserver.cpp").read_text(),
            )
            normal_gravitymon = (normal_dir / "gravitymon" / "src" / "web_brewing.cpp").read_text()
            self.assertNotIn("#if !defined(GMT154_C3_MDNS_OFF_DIAG)", normal_gravitymon)



    def test_profile_targets_inherit_repairs_without_mdns_disable(self):
        for target in ("gmt154-06-c3-ble-profile-act3", "gmt154-06-c3-ble-profile-act1"):
            with self.subTest(target=target), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                dependency = root / target
                copy_dependency_fixture(dependency)
                tft = load("tft")
                self.assertNotEqual(tft.apply_patch_for_env(target, root), "skipped")
                for name in ("single_app", "config_save"):
                    _, env = import_patcher_with_env(name, target, root)
                    self.assertTrue(env.middleware and env.actions)
                _, mdns = import_patcher_with_env("mdns", target, root)
                self.assertFalse(mdns.middleware or mdns.actions)
                framework = (dependency / "espframework/src/basewebserver.cpp").read_text()
                config = (dependency / "gravitymon/src/web_brewing.cpp").read_text()
                self.assertIn("Firmware upload is disabled on GMT154-06 C3.", framework)
                self.assertIn("Failed to save configuration", config)
                self.assertNotIn("GMT154_C3_MDNS_OFF_DIAG", framework)
                self.assertNotIn("GMT154_C3_MDNS_OFF_DIAG", config)

if __name__ == "__main__":
    unittest.main(verbosity=2)
