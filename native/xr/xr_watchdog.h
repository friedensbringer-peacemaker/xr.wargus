// Hänger-Diagnose: bleiben die Spielbilder aus, schreibt der XR-Thread den Aufrufstapel des
// Engine-Threads ins Sitzungslog (ohne Root, ohne Debugger). Der Engine-Thread meldet sich mit
// watchdogMainThread() an (erster Present); der XR-Thread ruft watchdogPoll() je Frame.
#pragma once

#include <cstdint>

namespace xrs {

// Im Engine-Thread aufrufen (idempotent).
void watchdogMainThread();
// Im XR-Thread: lastPresentNs = Zeit des letzten Spielbilds (CLOCK_MONOTONIC), what = Kontext fürs Log.
void watchdogPoll(uint64_t nowNs, uint64_t lastPresentNs, const char* what);

}  // namespace xrs
