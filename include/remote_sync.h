#pragma once

enum class RemoteSyncResult : unsigned char {
    Skipped,
    Success,
    Failed,
};

// Sync once on a network wake. A skipped result means no device token is
// provisioned; only an actual failed attempt should schedule a retry.
RemoteSyncResult remote_sync(bool retryAttempt = false);
// Finish after the weather task, piggybacking fresh runtime on the existing ACK.
RemoteSyncResult remote_finish_report();
void noteRemoteSyncFailure(const char* code);
void remote_recover_pending();
bool remote_config_unavailable();
