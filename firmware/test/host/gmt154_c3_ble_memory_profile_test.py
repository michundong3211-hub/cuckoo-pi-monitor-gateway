import importlib.util
import runpy
import subprocess
import shutil
import tempfile
import unittest
from pathlib import Path
ROOT = Path(__file__).parents[2]
SCRIPT = ROOT/'script/patch_nimble_gmt154_c3_ble_memory_profile.py'
FIXTURE = ROOT/'test/host/fixtures/NimBLEDevice-2.3.7.cpp'
TARGETS={'gmt154-06-c3-ble-profile-act3','gmt154-06-c3-ble-profile-act1'}
def load():
    if not SCRIPT.exists(): raise AssertionError('BLE profile patch not implemented')
    spec=importlib.util.spec_from_file_location('ble_memory_profile',SCRIPT)
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);return module
class Env(dict):
    def __init__(self,target,root):super().__init__(PIOENV=target);self.root=root;self.middleware=None;self.post=None
    def subst(self,value):return str(self.root)
    def AddBuildMiddleware(self,callback):self.middleware=callback
    def AddPostAction(self,target,callback):self.post=callback
class Node:
    def __init__(self,path):self.path=path
    def srcnode(self):return self
    def get_path(self):return str(self.path)

def function(source, signature):
    start=source.index(signature);begin=source.index('{',start);depth=0
    for pos in range(begin,len(source)):
        if source[pos]=='{':depth+=1
        elif source[pos]=='}':
            depth-=1
            if depth==0:return source[start:pos+1]
    raise AssertionError('Unbalanced real function')

NIMBLE_HARNESS = r"""
#include <cassert>
#include <string>
#include <vector>
#include <utility>
using esp_err_t=int;
constexpr int ESP_OK=0,ESP_ERR_NVS_NO_FREE_PAGES=1,ESP_ERR_NVS_NEW_VERSION_FOUND=2,ESP_BT_MODE_BLE=1;
#define ESP_PLATFORM 1
#define ESP_IDF_VERSION 50500
#define ESP_IDF_VERSION_VAL(a,b,c) ((a)*10000+(b)*100+(c))
#if TEST_C3
#define CONFIG_IDF_TARGET_ESP32C3 1
#define GMT154_06_C3 1
#else
#define CONFIG_IDF_TARGET_ESP32S3 1
#endif
#if TEST_PROFILE
#define GMT154_C3_BLE_MEMORY_PROFILE 1
#endif
#define MYNEWT_VAL(x) TEST_##x
#define TEST_BLE_MAX_CONNECTIONS 1
#define TEST_BLE_ROLE_BROADCASTER 1
#define TEST_BLE_ROLE_OBSERVER 1
#define TEST_BLE_LL_CFG_FEAT_LL_PRIVACY 0
#define CONFIG_BTDM_BLE_SCAN_DUPL 0
#define CONFIG_BT_LE_SCAN_DUPL 0
#define NIMBLE_LOGE(...) ((void)0)
struct esp_bt_controller_config_t {int ble_max_act;};
#define BT_CONTROLLER_INIT_CONFIG_DEFAULT() esp_bt_controller_config_t{3}
struct ble_store_status_event {};
struct Callbacks {int onStoreStatus(ble_store_status_event*,void*) {return 0;}};
struct HostConfig {
 void(*reset_cb)();void(*sync_cb)();int(*store_status_cb)(ble_store_status_event*,void*);
 int sm_io_cap,sm_bonding,sm_mitm,sm_sc,sm_our_key_dist,sm_their_key_dist;
} ble_hs_cfg{};
constexpr int BLE_HS_IO_NO_INPUT_OUTPUT=0,BLE_SM_PAIR_KEY_DIST_ENC=1,BLE_SM_PAIR_KEY_DIST_ID=2;
int ctrlError=0,enableError=0,hciError=0,portError=0,actualActivities=0;
int controllerCalls=0,enableCalls=0,hciCalls=0,portCalls=0,threadCalls=0;
std::vector<std::string> stages;
std::vector<std::pair<std::string,int>> results;
void gmt154LogHeapSnapshot(const char* stage) {stages.emplace_back(stage);}
void gmt154LogBleInitResult(const char* phase,int result) {results.emplace_back(phase,result);}
int nvs_flash_init() {return ESP_OK;}int nvs_flash_erase() {return ESP_OK;}
int esp_bt_controller_init(esp_bt_controller_config_t* p) {++controllerCalls;actualActivities=p->ble_max_act;return ctrlError;}
int esp_bt_controller_enable(int) {++enableCalls;return enableError;}
int esp_nimble_hci_init() {++hciCalls;return hciError;}
int nimble_port_init() {++portCalls;return portError;}
void nimble_port_freertos_init(void(*)(void*)) {++threadCalls;}
void ble_store_config_init() {}
void ble_npl_time_delay(int) {}
class NimBLEDevice {public:
 static bool m_initialized,m_synced;
 static Callbacks* m_pDeviceCallbacks;
 static bool init(const std::string&);
 static void onReset() {} static void onSync() {} static void host_task(void*) {}
 static void setDeviceName(const std::string&) {}
};
bool NimBLEDevice::m_initialized=false,NimBLEDevice::m_synced=true;
Callbacks callbacks;Callbacks* NimBLEDevice::m_pDeviceCallbacks=&callbacks;
"""
NIMBLE_MAIN = r"""
bool has(const char* s) {for(const auto& stage:stages)if(stage==s)return true;return false;}
void reset() {NimBLEDevice::m_initialized=false;ctrlError=enableError=hciError=portError=0;controllerCalls=enableCalls=hciCalls=portCalls=threadCalls=0;stages.clear();results.clear();}
int main() {
 reset();assert(NimBLEDevice::init(""));
 assert(actualActivities==((TEST_C3 && TEST_PROFILE)?GMT154_C3_BLE_CONTROLLER_ACTIVITIES:3));
 assert(controllerCalls==1 && enableCalls==1 && portCalls==1 && threadCalls==1);
 if(TEST_C3 && TEST_PROFILE) {
  assert(has("ble.profile.controller_init.before") && has("ble.profile.controller_init.after"));
  assert(has("ble.profile.controller_enable.before") && has("ble.profile.controller_enable.after"));
  assert(has("ble.profile.host_port_init.before") && has("ble.profile.host_port_init.after"));
  assert(has("ble.profile.host_thread_call.before") && has("ble.profile.host_thread_call.after"));
  if(CONFIG_BT_NIMBLE_LEGACY_VHCI_ENABLE)assert(hciCalls==1 && has("ble.profile.legacy_hci.before") && has("ble.profile.legacy_hci.after"));
  else assert(hciCalls==0 && has("ble.profile.legacy_hci.skip") && !has("ble.profile.legacy_hci.before"));
  assert(results.size()==(CONFIG_BT_NIMBLE_LEGACY_VHCI_ENABLE?4:3));
 } else assert(stages.empty() && results.empty());
 reset();portError=258;
 bool initialized=NimBLEDevice::init("");
 if(TEST_C3 && TEST_PROFILE) {assert(!initialized && threadCalls==0 && !NimBLEDevice::m_initialized);assert(results.back()==std::make_pair(std::string("host_port_init"),258));}
 else assert(initialized && threadCalls==1);
 reset();ctrlError=257;assert(!NimBLEDevice::init(""));assert(enableCalls==0 && portCalls==0 && threadCalls==0);
 if(TEST_C3 && TEST_PROFILE)assert(results.back()==std::make_pair(std::string("controller_init"),257));
 reset();enableError=259;assert(!NimBLEDevice::init(""));assert(portCalls==0 && threadCalls==0);
 if(TEST_C3 && TEST_PROFILE)assert(results.back()==std::make_pair(std::string("controller_enable"),259));
 if(CONFIG_BT_NIMBLE_LEGACY_VHCI_ENABLE) {
  reset();hciError=260;assert(!NimBLEDevice::init(""));assert(portCalls==0 && threadCalls==0);
  if(TEST_C3 && TEST_PROFILE)assert(results.back()==std::make_pair(std::string("legacy_hci"),260));
 }
}
"""
APP_HARNESS = r"""
#include <cassert>
#define F(s) s
#define CR "\n"
int gets=0,scans=0,errors=0;
bool initResult=false;
struct Scan {void setScanCallbacks(void*){} void setMaxResults(int){} void setActiveScan(bool){} void setInterval(int){} void setWindow(int){}} instance;
struct NimBLEDevice {static bool init(const char*) {return initResult;}static Scan* getScan(){++gets;return &instance;}};
struct Logger {void notice(const char*,const char*){} void error(const char*) {++errors;}} Log;
void gmt154LogHeapSnapshot(const char*) {}
struct BleScanner {Scan* _bleScan=&instance;void* _deviceCallbacks=nullptr;bool _activeScan=true;void init();bool scan(){++scans;return true;}};
"""
APP_MAIN = r"""
int main(){BleScanner scanner;scanner.init();
#if defined(GMT154_06_C3)
assert(gets==0 && scans==0 && errors==1 && scanner._bleScan==nullptr);
#else
assert(gets==1 && scans==1 && errors==0);
#endif
gets=scans=errors=0;initResult=true;scanner.init();assert(gets==1 && scans==1 && errors==0 && scanner._bleScan==&instance);}
"""

class Profile(unittest.TestCase):
    def test_01_full_source_pin_idempotent_and_crlf(self):
        patch=load()
        for newline in ['\n','\r\n']:
            with tempfile.TemporaryDirectory() as tmp:
                path=Path(tmp)/'NimBLEDevice.cpp';original=FIXTURE.read_text().replace('\n',newline).encode();path.write_bytes(original)
                self.assertEqual(patch.patch_source(path),'changed');first=path.read_bytes()
                self.assertEqual(patch.patch_source(path),'already');self.assertEqual(first,path.read_bytes())
                restored=first.decode().replace('\r\n','\n')
                for old,new in patch.PATCHES:restored=restored.replace(new,old)
                self.assertEqual(restored,FIXTURE.read_text())
    def test_02_unknown_partial_duplicate_and_moved_owned_blocks_fail_closed(self):
        patch=load();original=FIXTURE.read_text();patched=original
        for old,new in patch.PATCHES:patched=patched.replace(old,new)
        variants=[original+'// drift\n',patched+'// drift\n',patched+patch.PATCHES[0][1],patched.replace(patch.PATCHES[1][1],patch.PATCHES[1][0]),patched.replace('GMT154_C3_BLE_PROFILE_BEGIN','BROKEN',1)]
        new=patch.PATCHES[0][1];variants.append(patched.replace(new,patch.PATCHES[0][0],1)+new)
        for source in variants:
            with self.subTest(tail=source[-30:]),tempfile.TemporaryDirectory() as tmp:
                path=Path(tmp)/'NimBLEDevice.cpp';path.write_text(source);before=path.read_bytes()
                with self.assertRaises(patch.PatchError):patch.patch_source(path)
                self.assertEqual(before,path.read_bytes())
    def test_03_target_isolation_deferred_main_source_and_post_verification(self):
        for target in ['gmt154-06-c3','gmt154-06-c3-mdns-off','gmt154-06-c3-mdns-off-ble-init-off','gmt154-06-c3-mdns-off-ble-stage-diag','gmt154-06','unit']+sorted(TARGETS):
            with self.subTest(target=target),tempfile.TemporaryDirectory() as tmp:
                root=Path(tmp);env=Env(target,root)
                runpy.run_path(str(SCRIPT),init_globals={'Import':lambda _:None,'env':env})
                if target not in TARGETS:self.assertIsNone(env.middleware);continue
                self.assertIsNotNone(env.middleware)
                with self.assertRaises(RuntimeError):env.post([],[],env)
                header=root/target/'NimBLE-Arduino/src/NimBLEDevice.cpp';header.parent.mkdir(parents=True);header.write_bytes(FIXTURE.read_bytes())
                env.middleware(env,Node(root/'src/main_gateway.cpp'))
                self.assertIn('GMT154_C3_BLE_PROFILE_BEGIN',header.read_text())
                env.post([],[],env)
                header.write_text(header.read_text()+'// drift\n')
                with self.assertRaises(RuntimeError):env.post([],[],env)

    def test_04_real_patched_init_executes_activities_phases_errors_and_isolation(self):
        patch=load();source=FIXTURE.read_text()
        for old,new in patch.PATCHES:source=source.replace(old,new)
        init=function(source,'bool NimBLEDevice::init(')
        for c3,profile,activities,legacy in [(1,1,3,1),(1,1,1,1),(1,1,1,0),(1,0,1,1),(0,1,1,1)]:
            with self.subTest(c3=c3,profile=profile,activities=activities,legacy=legacy),tempfile.TemporaryDirectory() as tmp:
                path=Path(tmp)/'init.cpp';path.write_text(NIMBLE_HARNESS+init+NIMBLE_MAIN)
                exe=Path(tmp)/'init.exe'
                result=subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',f'-DTEST_C3={c3}',f'-DTEST_PROFILE={profile}',f'-DGMT154_C3_BLE_CONTROLLER_ACTIVITIES={activities}',f'-DCONFIG_BT_NIMBLE_LEGACY_VHCI_ENABLE={legacy}',str(path),'-o',str(exe)],capture_output=True,text=True)
                self.assertEqual(result.returncode,0,result.stderr)
                result=subprocess.run([str(exe)],capture_output=True,text=True);self.assertEqual(result.returncode,0,result.stderr)
    def test_05_real_app_init_false_never_gets_scan_and_c3_stage_diag_preserved(self):
        init=function((ROOT/'src/ble_gateway.cpp').read_text(encoding='utf-8'),'void BleScanner::init()')
        for flags in [[],['-DGMT154_06_C3=1'],['-DGMT154_06_C3=1','-DGMT154_C3_BLE_STAGE_DIAG=1']]:
            with self.subTest(flags=flags),tempfile.TemporaryDirectory() as tmp:
                path=Path(tmp)/'app.cpp';path.write_text(APP_HARNESS+init+APP_MAIN)
                exe=Path(tmp)/'app.exe';result=subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',*flags,str(path),'-o',str(exe)],capture_output=True,text=True)
                self.assertEqual(result.returncode,0,result.stderr)
                result=subprocess.run([str(exe)],capture_output=True,text=True);self.assertEqual(result.returncode,0,result.stderr)


    def test_06_profile_envs_keep_same_ledger_and_isolate_activity_override(self):
        source=(ROOT/'platformio.ini').read_text(encoding='utf-8')
        sections=[]
        for target,activity in [('gmt154-06-c3-ble-profile-act3',3),('gmt154-06-c3-ble-profile-act1',1)]:
            section=source.split('[env:'+target+']',1)[1].split('\n[',1)[0]
            self.assertIn('extends = env:gmt154-06-c3',section)
            self.assertIn('${env:gmt154-06-c3.extra_scripts}',section)
            self.assertIn('pre:script/patch_nimble_gmt154_c3_ble_memory_profile.py',section)
            self.assertIn('-D GMT154_C3_BLE_MEMORY_PROFILE=1',section)
            self.assertIn('-D GMT154_C3_BLE_STAGE_DIAG=1',section)
            self.assertIn('-D GMT154_C3_BLE_CONTROLLER_ACTIVITIES='+str(activity),section)
            self.assertNotIn('MDNS_OFF',section)
            sections.append('\n'.join(line for line in section.splitlines() if not line.lstrip().startswith(';')).strip().replace('ACTIVITIES='+str(activity),'ACTIVITIES=TEST'))
        self.assertEqual(sections[0],sections[1])
        for target in ['gmt154-06-c3','gmt154-06-c3-mdns-off','gmt154-06-c3-mdns-off-ble-init-off','gmt154-06-c3-mdns-off-ble-stage-diag']:
            section=source.split('[env:'+target+']',1)[1].split('\n[',1)[0]
            self.assertNotIn('GMT154_C3_BLE_MEMORY_PROFILE',section)
            self.assertNotIn('GMT154_C3_BLE_CONTROLLER_ACTIVITIES',section)
            self.assertNotIn('patch_nimble_gmt154_c3_ble_memory_profile.py',section)

if __name__=='__main__':unittest.main()
