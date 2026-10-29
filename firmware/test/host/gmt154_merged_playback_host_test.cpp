#include <cassert>
#include "../../src/gateway_audio.hpp"
static uint32_t nowValue;
static uint32_t nowMs() { return nowValue; }
struct Sink : GatewayAudioPlaybackSink {
 int calls=0, silence=0; int failAt=-1; GatewayAudioPlaybackStepResult fail=GatewayAudioPlaybackStepResult::Cancelled;
 GatewayAudioPlaybackStepResult step(uint16_t duration) { ++calls; if(calls==failAt) return fail; nowValue+=duration; return GatewayAudioPlaybackStepResult::Completed; }
 GatewayAudioPlaybackStepResult writeTone(const GatewayAudioQueueItem&,uint16_t,uint16_t d) override { return step(d); }
 GatewayAudioPlaybackStepResult waitGap(const GatewayAudioQueueItem&,uint16_t d) override { return step(d); }
 void writeSilence(const GatewayAudioQueueItem&) override { ++silence; }
};
int main(){ GatewayAudioQueueItem item{}; item.decision=gatewayAudioDecisionForAlarm(GatewayAlarmType::Offline);
 Sink s; nowValue=0xfffffff0u; assert(gatewayAudioPlayMergedWindow(item,&s,nowMs)==GatewayAudioPlaybackStepResult::Completed); assert(uint32_t(nowValue-0xfffffff0u)==15000); assert(s.silence==1);
 Sink stop; nowValue=0; stop.failAt=4; assert(gatewayAudioPlayMergedWindow(item,&stop,nowMs)==GatewayAudioPlaybackStepResult::Cancelled); assert(stop.silence==1);
 Sink fail; nowValue=0; fail.failAt=2; fail.fail=GatewayAudioPlaybackStepResult::WriteFailed; assert(gatewayAudioPlayMergedWindow(item,&fail,nowMs)==GatewayAudioPlaybackStepResult::WriteFailed); assert(fail.silence==1);
}
