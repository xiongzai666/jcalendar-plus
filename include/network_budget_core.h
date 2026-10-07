#pragma once
#include <cstdint>
#include <cstddef>
struct NetworkBudget {
    uint32_t started = 0, duration = 75000;
    void begin(uint32_t now, uint32_t limit = 75000) { started = now; duration = limit; }
    uint32_t remaining(uint32_t now) const {
        const uint32_t elapsed = now - started;
        return elapsed < duration ? duration - elapsed : 0;
    }
    bool canRequest(uint32_t now) const { return remaining(now) >= 4000; }
    uint32_t phaseTimeout(uint32_t now) const {
        const uint32_t slice = remaining(now) / 4;
        return slice > 5000 ? 5000 : slice;
    }
    bool acceptBody(uint32_t now, size_t existing, size_t addition, size_t limit) const {
        return remaining(now) && existing <= limit && addition <= limit - existing;
    }
};
struct BlockingTransportLimits {
    bool allowed = false;
    uint32_t connectMs = 0, handshakeSeconds = 0;
};
inline BlockingTransportLimits blockingTransportLimits(const NetworkBudget& budget, uint32_t now, int32_t requested) {
    BlockingTransportLimits out;
    if (!budget.canRequest(now)) return out;
    const uint32_t slice = budget.phaseTimeout(now);
    out.connectMs = requested > 0 && uint32_t(requested) < slice ? uint32_t(requested) : slice;
    out.handshakeSeconds = out.connectMs / 1000;
    if (!out.handshakeSeconds) out.handshakeSeconds = 1;
    out.allowed = out.connectMs + out.handshakeSeconds * 1000 <= budget.remaining(now);
    return out;
}
