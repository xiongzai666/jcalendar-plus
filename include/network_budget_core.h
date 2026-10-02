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
