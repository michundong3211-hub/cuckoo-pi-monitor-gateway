import sys
import os
import subprocess
from pathlib import Path
ROOT = Path(__file__).parents[2]
PIO = Path.home()/'.platformio'
PYTHON = PIO/'penv/Scripts/python.exe'
if Path(sys.executable).resolve() != PYTHON.resolve():
    raise SystemExit(subprocess.call([str(PYTHON),str(Path(__file__).resolve())]))
sys.path.insert(0,str(next((PIO/'packages/tool-scons').glob('scons-local-*'))))
import unittest
from platformio.project.config import ProjectConfig
from platformio.builder.tools import piobuild
from SCons.Script import Environment

def env_for(target):
    cfg=ProjectConfig(str(ROOT/'platformio.ini'))
    env=Environment(tools=[], ENV={**os.environ, "PATH": str(PYTHON.parent)+os.pathsep+os.environ.get("PATH","")})
    env.AddMethod(piobuild.ParseFlagsExtended)
    piobuild.ProcessFlags(env,cfg.get('env:'+target,'build_flags'))
    piobuild.ProcessUnFlags(env,cfg.get('env:'+target,'build_unflags',[]))
    return env

def memory(env):
    return [d[1] for d in env.get('CPPDEFINES',[]) if isinstance(d,(tuple,list)) and d[0]=='LV_MEM_SIZE']

class Config(unittest.TestCase):
    def test_actual_platformio_config_scopes(self):
        for target in ['gmt154-06-c3','gmt154-06-c3-mdns-off','gmt154-06-c3-mdns-off-ble-init-off','gmt154-06-c3-mdns-off-ble-stage-diag']:
            with self.subTest(target=target):self.assertEqual(memory(env_for(target)),['24*1024'])
        for target in ['gmt154-06','gateway35s3wave-tft-sd']:
            with self.subTest(target=target):self.assertEqual(memory(env_for(target)),['32*1024'])
    def test_actual_process_unflags_exact_name_and_value(self):
        env=Environment(tools=[], ENV={**os.environ, "PATH": str(PYTHON.parent)+os.pathsep+os.environ.get("PATH","")});env.AddMethod(piobuild.ParseFlagsExtended)
        piobuild.ProcessFlags(env,['-D LV_MEM_SIZE=32*1024','-D LV_MEM_SIZE=24*1024'])
        piobuild.ProcessUnFlags(env,'-D LV_MEM_SIZE=32*1024')
        self.assertEqual(memory(env),['24*1024'])
        # Negative control: bare-name removal would remove BOTH, not an override.
        piobuild.ProcessUnFlags(env,'-D LV_MEM_SIZE')
        self.assertEqual(memory(env),[])
if __name__=='__main__':unittest.main()
