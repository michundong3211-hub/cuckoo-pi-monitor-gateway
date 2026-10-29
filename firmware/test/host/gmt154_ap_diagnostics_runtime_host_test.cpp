#define GMT154_06 1
#define GMT154_06_C3 1
#include <esp_heap_caps.h>
inline size_t heap_caps_get_minimum_free_size(unsigned) { return 100; }
inline bool heap_caps_check_integrity(unsigned, bool) { return true; }
#include "../../src/gmt154_heap_diagnostics.cpp"
#include <cassert>
int main() {
  registrationError=257; // Native registration failure: one log, snapshot, continued execution.
  gmt154StartApDiagnostics(); gmt154StartApDiagnostics();
  assert(registrationCalls==1 && fakeLines.size()==2);
  assert(fakeLines[0].find("registration_failed err=257")!=std::string::npos);
  assert(fakeLines[1].find("stage=startup")!=std::string::npos);
  // Separate successful startup scenario without allocating callback state.
  apRegistrationAttempted=false; registrationError=0; fakeLines.clear();
  gmt154StartApDiagnostics();
  assert(registrationCalls==2 && fakeLines.size()==2);
  const auto logs=fakeLines.size();
  fakeMillis=7;
  fakeHandler(nullptr,WIFI_EVENT,WIFI_EVENT_AP_START,nullptr);
  fakeHandler(nullptr,WIFI_EVENT,WIFI_EVENT_AP_STACONNECTED,nullptr);
  wifi_event_ap_stadisconnected_t ap{8}; wifi_event_sta_disconnected_t sta{201};
  fakeHandler(nullptr,WIFI_EVENT,WIFI_EVENT_AP_STADISCONNECTED,&ap);
  fakeHandler(nullptr,WIFI_EVENT,WIFI_EVENT_STA_DISCONNECTED,&sta);
  fakeHandler(nullptr,WIFI_EVENT,WIFI_EVENT_AP_STOP,nullptr);
  assert(apEvents.starts==1 && apEvents.stops==1 && apEvents.associations==1);
  assert(apEvents.apReason==8 && apEvents.staReason==201 && apEvents.lastMs==7);
  assert(fakeLines.size()==logs && criticalDepth==0);
  const auto entries=criticalEntries;
  fakeHandler(nullptr,"other",WIFI_EVENT_AP_START,nullptr);
  fakeHandler(nullptr,WIFI_EVENT,999,nullptr);
  assert(entries==criticalEntries);
  fakeHandler(nullptr,WIFI_EVENT,WIFI_EVENT_AP_STADISCONNECTED,nullptr);
  fakeHandler(nullptr,WIFI_EVENT,WIFI_EVENT_STA_DISCONNECTED,nullptr);
  assert(apEvents.apReason==-1 && apEvents.staReason==-1);
  fakeModeErr=1;fakeClientsErr=2;fakeIpError=3;
  fakeMillis=29999;gmt154PollHeapDiagnostics();assert(fakeLines.size()==logs);
  fakeMillis=30000;gmt154PollHeapDiagnostics();assert(fakeLines.size()==logs+2);
  assert(fakeLines.back().find("mode=unknown mode_err=1 clients=unknown clients_err=2 ap_ip=unknown ip_err=3")!=std::string::npos);
  fakeNetifExists=false;fakeMillis=60000;gmt154PollHeapDiagnostics();
  assert(fakeLines.back().find("ip_err=261")!=std::string::npos);
  assert(criticalDepth==0);
}
