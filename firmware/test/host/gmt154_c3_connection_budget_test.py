import hashlib
import importlib.util
import runpy
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).parents[2]
SCRIPT = ROOT / 'script/patch_nimble_gmt154_c3_connection_budget.py'
FIXTURE = ROOT / 'test/host/fixtures/nimconfig-2.3.7.h'
TARGETS = {'gmt154-06-c3-ble-profile-act3', 'gmt154-06-c3-ble-profile-act1', 'gmt154-06-c3', 'gmt154-06-c3-mdns-off', 'gmt154-06-c3-mdns-off-ble-init-off', 'gmt154-06-c3-mdns-off-ble-stage-diag'}


def load():
    if not SCRIPT.exists():
        raise AssertionError('C3 connection-budget patch is not implemented')
    spec = importlib.util.spec_from_file_location('connection_budget', SCRIPT)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class FakeEnv(dict):
    def __init__(self, target, root):
        super().__init__(PIOENV=target)
        self.root, self.middleware, self.post = root, None, None
    def subst(self, value):
        return str(self.root)
    def AddBuildMiddleware(self, callback):
        self.middleware = callback
    def AddPostAction(self, target, callback):
        self.post = callback


class Node:
    def __init__(self, path): self.path = path
    def srcnode(self): return self
    def get_path(self): return str(self.path)


class BudgetTest(unittest.TestCase):
    def test_01_patch_is_idempotent_and_preserves_line_endings(self):
        p = load()
        for newline in ('\n', '\r\n'):
            with self.subTest(newline=repr(newline)), tempfile.TemporaryDirectory() as tmp:
                path = Path(tmp) / 'nimconfig.h'
                original = FIXTURE.read_text().replace('\n', newline).encode()
                path.write_bytes(original)
                self.assertEqual(p.patch_source(path), 'changed')
                patched = path.read_bytes()
                self.assertEqual(p.patch_source(path), 'already')
                self.assertEqual(path.read_bytes(), patched)
                self.assertEqual(patched.count(p.PATCH_BLOCK.replace('\n', newline).encode()), 1)
                self.assertEqual(patched.replace(p.PATCH_BLOCK.replace('\n', newline).encode(), b''), original)

    def test_02_unknown_partial_duplicate_and_post_override_are_rejected(self):
        p = load()
        original = FIXTURE.read_text()
        patched = original.replace(p.ANCHOR, p.ANCHOR + p.PATCH_BLOCK)
        variants = [original + '// drift\n', original.replace('Arduino user-config', 'Different user-config'),
                    patched + '#define CONFIG_BT_NIMBLE_MAX_CONNECTIONS 3\n',
                    patched.replace('CONFIG_NIMBLE_MAX_CONNECTIONS 1', 'CONFIG_NIMBLE_MAX_CONNECTIONS 2'),
                    patched + p.PATCH_BLOCK, original + '// GMT154_C3_CONNECTION_BUDGET_BEGIN\n']
        for variant in variants:
            with self.subTest(variant=variant[-70:]), tempfile.TemporaryDirectory() as tmp:
                path = Path(tmp) / 'nimconfig.h'
                path.write_text(variant)
                before = path.read_bytes()
                with self.assertRaises(p.PatchError): p.patch_source(path)
                self.assertEqual(before, path.read_bytes())

    def test_03_real_c_and_cpp_preprocessing_sdk_and_legacy_alias(self):
        p = load()
        for compiler in ('gcc', 'g++'):
            self.assertIsNotNone(shutil.which(compiler))
            for alias in ('3', 'CONFIG_BT_NIMBLE_MAX_CONNECTIONS'):
                for c3 in (False, True):
                    with self.subTest(compiler=compiler, alias=alias, c3=c3), tempfile.TemporaryDirectory() as tmp:
                        root = Path(tmp)
                        header = root / 'nimconfig.h'
                        header.write_bytes(FIXTURE.read_bytes())
                        p.patch_source(header)
                        (root / 'sdkconfig.h').write_text('#define CONFIG_BT_NIMBLE_MAX_CONNECTIONS 3\n#define CONFIG_NIMBLE_MAX_CONNECTIONS ' + alias + '\n' + ''.join('#define CONFIG_BT_NIMBLE_ROLE_' + role + ' 1\n' for role in ('CENTRAL', 'PERIPHERAL', 'BROADCASTER', 'OBSERVER')))
                        (root / 'nimconfig_rename.h').write_text('')
                        expected = 1 if c3 else 3
                        source = '#include "nimconfig.h"\n#define MYNEWT_VAL_BLE_MAX_CONNECTIONS CONFIG_BT_NIMBLE_MAX_CONNECTIONS\n' + f'#if CONFIG_BT_NIMBLE_MAX_CONNECTIONS != {expected} || CONFIG_NIMBLE_MAX_CONNECTIONS != {expected} || MYNEWT_VAL_BLE_MAX_CONNECTIONS != {expected}\n#error wrong connection count\n#endif\n' + '#if CONFIG_BT_NIMBLE_ROLE_CENTRAL != 1 || CONFIG_BT_NIMBLE_ROLE_PERIPHERAL != 1 || CONFIG_BT_NIMBLE_ROLE_BROADCASTER != 1 || CONFIG_BT_NIMBLE_ROLE_OBSERVER != 1\n#error roles changed\n#endif\n' + f'#if CONFIG_BT_NIMBLE_MAX_CONNECTIONS + CONFIG_BT_NIMBLE_ROLE_OBSERVER + CONFIG_BT_NIMBLE_ROLE_BROADCASTER != {expected + 2}\n#error wrong activities\n#endif\n'
                        command = [compiler, '-E', '-Werror', '-DESP_PLATFORM', '-I' + str(root), '-x', 'c++' if compiler == 'g++' else 'c', '-']
                        if c3: command += ['-DGMT154_06_C3=1']
                        result = subprocess.run(command, input=source, text=True, capture_output=True)
                        self.assertEqual(result.returncode, 0, result.stderr)

    def test_04_hook_targets_and_delayed_dependencies(self):
        p = load()
        self.assertEqual(p.TARGET_ENVS, TARGETS)
        for target in TARGETS:
            with self.subTest(target=target), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                env = FakeEnv(target, root)
                runpy.run_path(str(SCRIPT), init_globals={'Import': lambda name: None, 'env': env})
                with self.assertRaises(RuntimeError): env.post(None, None, env)
                source = root / target / 'NimBLE-Arduino/src/NimBLEDevice.cpp'
                source.parent.mkdir(parents=True)
                source.write_text('')
                header = source.parent / 'nimconfig.h'
                header.write_bytes(FIXTURE.read_bytes())
                # Application consumers must also see the patched header if
                # dependencies appeared only after extra_scripts registration.
                env.middleware(env, Node(root / 'src/main_gateway.cpp'))
                self.assertIn(p.PATCH_BLOCK, header.read_text())
                env.middleware(env, Node(source))
                env.post(None, None, env)
                self.assertIn(p.PATCH_BLOCK, header.read_text())
                header.write_text(header.read_text() + '// drift\n')
                with self.assertRaises(RuntimeError): env.post(None, None, env)

    def test_05_nontarget_and_unexpected_source(self):
        p = load()
        with tempfile.TemporaryDirectory() as tmp:
            env = FakeEnv('gateway32s3wave-tft-sd', Path(tmp))
            runpy.run_path(str(SCRIPT), init_globals={'Import': lambda name: None, 'env': env})
            self.assertIsNone(env.middleware)
            env = FakeEnv('gmt154-06-c3', Path(tmp))
            runpy.run_path(str(SCRIPT), init_globals={'Import': lambda name: None, 'env': env})
            with self.assertRaises(RuntimeError): env.middleware(env, Node(Path(tmp) / 'other/NimBLE-Arduino/src/NimBLEDevice.cpp'))

    def test_06_production_hook_registration(self):
        load()
        ini = (ROOT / 'platformio.ini').read_text(encoding='utf-8-sig')
        section = ini.split('[env:gmt154-06-c3]')[1].split('\n[')[0]
        self.assertEqual(section.count('pre:script/patch_nimble_gmt154_c3_connection_budget.py'), 1)
        self.assertIn('test/host/gmt154_c3_connection_budget_test.py', (ROOT / 'script/run_host_tests.ps1').read_text())

if __name__ == '__main__': unittest.main()
