#pragma once

#include <stdint.h>
#include <time.h>

#include "remote_sync.h"

constexpr time_t REMOTE_RETRY_DELAY_SECONDS = 10 * 60;

enum class RemoteRetryPhase : uint8_t {
    Idle = 0,
    Pending = 1,
    Exhausted = 2,
};

struct RemoteRetryState {
    RemoteRetryPhase phase;
    time_t dueAt;
};

inline void remoteRetryBeginRegularCycle(RemoteRetryState& state) {
    if (state.phase == RemoteRetryPhase::Exhausted) state = {};
}

inline bool remoteRetryIsDue(const RemoteRetryState& state, time_t now,
                             bool plannedRetryWake) {
    return state.phase == RemoteRetryPhase::Pending &&
           (plannedRetryWake || now >= state.dueAt);
}

inline void remoteRetryRecord(RemoteRetryState& state, RemoteSyncResult result,
                              bool dueAttempt, time_t now) {
    if (result == RemoteSyncResult::Skipped) return;
    if (result == RemoteSyncResult::Success) {
        state = {};
    } else if (dueAttempt) {
        // One delayed attempt per regular network cycle, even if it fails.
        state.phase = RemoteRetryPhase::Exhausted;
        state.dueAt = 0;
    } else if (state.phase == RemoteRetryPhase::Idle) {
        state.phase = RemoteRetryPhase::Pending;
        state.dueAt = now + REMOTE_RETRY_DELAY_SECONDS;
    }
}
