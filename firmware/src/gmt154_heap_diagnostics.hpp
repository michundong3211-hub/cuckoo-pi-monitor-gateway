#pragma once

#if defined(GMT154_06)
// Called in task context only; no allocation or network requests by this probe.
void gmt154LogHeapSnapshot(const char* stage);
void gmt154PollHeapDiagnostics();
#else
inline void gmt154LogHeapSnapshot(const char*) {}
inline void gmt154PollHeapDiagnostics() {}
#endif

#if defined(GMT154_06_C3)
// Once after Wi-Fi startup. Native event registration may allocate; failure is nonfatal.
void gmt154StartApDiagnostics();
#else
inline void gmt154StartApDiagnostics() {}
#endif

#if defined(GMT154_06_C3) && GMT154_06_C3 && defined(GMT154_C3_BLE_MEMORY_PROFILE) && GMT154_C3_BLE_MEMORY_PROFILE
// Fixed-stack result log, independent of the dependency's NIMBLE_LOG_LEVEL.
void gmt154LogBleInitResult(const char* phase, int result);
#endif
