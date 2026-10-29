#include <cassert>
#include "../../src/gmt154_merged_audio.hpp"
#include "../../src/gateway_alarm.hpp"
int main() {
 Gmt154MergedAudioCycle c;
 assert(!c.update(false,0)); assert(c.update(true,100));
 assert(!c.update(true,299999)); assert(!c.update(true,300099)); assert(c.update(true,300100));
 assert(!c.update(true,300101)); assert(!c.update(false,300102)); assert(c.update(true,300103));
 Gmt154MergedAudioCycle w; assert(w.update(true,0xfffffff0u)); assert(w.update(true,0xfffffff0u+300000u));
 GatewayAlarmTracker t; GatewayAudibleSummary s{}; assert(t.tryCopyAudibleSummary(&s)&&!s.any);
 assert(t.beginAudibleUpdate());
 assert(!t.tryCopyAudibleSummary(&s));
 t.endAudibleUpdate(); assert(t.tryCopyAudibleSummary(&s)&&!s.any);
 t.setExternalCondition(GatewayAlarmType::CuckooOffline,"a",true,0); assert(t.tryCopyAudibleSummary(&s)&&s.any&&!s.temperature);
 t.setExternalCondition(GatewayAlarmType::CuckooOffline,"a",true,0,true); assert(t.tryCopyAudibleSummary(&s)&&!s.any);
 t.setExternalCondition(GatewayAlarmType::CuckooOffline,"a",true,0,false); assert(t.tryCopyAudibleSummary(&s)&&s.any);
 t.setExternalCondition(GatewayAlarmType::CuckooBeerDeviation,"b",true,0); assert(t.tryCopyAudibleSummary(&s)&&s.temperature);
 auto event=t.schedule(GatewayAlarmType::CuckooOffline,"a",1); assert(event.should_play);
 assert(t.closeExact(event.event.event_token)==GatewayAlarmCloseResult::Closed);
 assert(t.tryCopyAudibleSummary(&s)&&s.any&&s.temperature);
 t.setExternalCondition(GatewayAlarmType::CuckooOffline,"a",false,0); assert(t.tryCopyAudibleSummary(&s)&&s.any);
 t.setExternalCondition(GatewayAlarmType::CuckooBeerDeviation,"b",false,0); assert(t.tryCopyAudibleSummary(&s)&&!s.any);
 t.setExternalCondition(GatewayAlarmType::CuckooOffline,"a",true,0); auto again=t.schedule(GatewayAlarmType::CuckooOffline,"a",2); assert(again.should_play); assert(t.tryCopyAudibleSummary(&s)&&s.any);
 // Cancellation while DMA is being silenced ends the old session even if
 // a new condition appears before the audio loop observes the empty state.
 Gmt154MergedAudioCycle resumed; assert(resumed.update(true,42));
 resumed.playbackCancelled(); assert(resumed.update(true,44));
}
