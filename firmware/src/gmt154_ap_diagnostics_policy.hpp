#pragma once
#include <cstdint>
#include <cstdio>
namespace gmt154 {
constexpr bool shouldDiscoverMdns(bool c3, bool measurement, bool connected) {
  return !c3 || (measurement && connected);
}
enum class ApEvent : uint32_t { start = 1, stop, associated, apDisconnected, staDisconnected };
struct ApEventState {
  uint32_t starts = 0, stops = 0, associations = 0, apDisconnects = 0, staDisconnects = 0;
  uint32_t lastEvent = 0, lastMs = 0;
  int32_t apReason = -1, staReason = -1;
};
inline void recordApEvent(ApEventState& s, ApEvent event, uint32_t ms,
                          bool reasonKnown, int32_t reason) {
  s.lastEvent = static_cast<uint32_t>(event); s.lastMs = ms;
  switch (event) {
    case ApEvent::start: ++s.starts; break;
    case ApEvent::stop: ++s.stops; break;
    case ApEvent::associated: ++s.associations; break;
    case ApEvent::apDisconnected: ++s.apDisconnects; s.apReason = reasonKnown ? reason : -1; break;
    case ApEvent::staDisconnected: ++s.staDisconnects; s.staReason = reasonKnown ? reason : -1; break;
  }
}
constexpr unsigned apLineCapacity = 440;
// Status 0 means query success. Failures stay explicit; they are not client count 0.
inline int formatApSnapshot(char* out, unsigned capacity, const char* stage,
                            const ApEventState& s, int modeErr, int mode,
                            uint32_t clients, int clientsErr, uint32_t ip, int ipErr) {
  char modeText[12] = "unknown", clientText[12] = "unknown", ipText[16] = "unknown";
  if (modeErr == 0) std::snprintf(modeText, sizeof(modeText), "%d", mode);
  if (clientsErr == 0) std::snprintf(clientText, sizeof(clientText), "%lu", static_cast<unsigned long>(clients));
  if (ipErr == 0) std::snprintf(ipText, sizeof(ipText), "%u.%u.%u.%u",
      static_cast<unsigned>(ip & 255), static_cast<unsigned>((ip >> 8) & 255),
      static_cast<unsigned>((ip >> 16) & 255), static_cast<unsigned>((ip >> 24) & 255));
  return std::snprintf(out, capacity,
      "APDIAG: stage=%s mode=%s mode_err=%d clients=%s clients_err=%d ap_ip=%s ip_err=%d "
      "start=%lu stop=%lu assoc=%lu ap_disc=%lu sta_disc=%lu last_event=%lu last_ms=%lu ap_reason=%ld sta_reason=%ld",
      stage, modeText, modeErr, clientText, clientsErr, ipText, ipErr,
      static_cast<unsigned long>(s.starts), static_cast<unsigned long>(s.stops),
      static_cast<unsigned long>(s.associations), static_cast<unsigned long>(s.apDisconnects),
      static_cast<unsigned long>(s.staDisconnects), static_cast<unsigned long>(s.lastEvent),
      static_cast<unsigned long>(s.lastMs), static_cast<long>(s.apReason), static_cast<long>(s.staReason));
}
}  // namespace gmt154
