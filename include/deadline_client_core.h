#pragma once
#include <cstddef>
#include <cstdint>
#include <atomic>
#include "network_budget_core.h"

// Guard every virtual Stream read, including HTTPClient's header parser.
// The transport must also disable its Stream idle wait when the budget expires.
template<class Transport, class Clock>
class DeadlineClient : public Transport {
    const NetworkBudget& budget;
    const std::atomic<bool>* cancellation = nullptr;
    bool expired() {
        if (!(cancellation && cancellation->load()) && budget.remaining(Clock::now())) return false;
        Transport::stop();
        Transport::disableReadWait();
        return true;
    }
public:
    explicit DeadlineClient(const NetworkBudget& value) : budget(value) {}
    void setCancellation(const std::atomic<bool>* flag) { cancellation = flag; }
    using Transport::connect;
    int connect(const char* host, uint16_t port) override {
        return expired() || !budget.canRequest(Clock::now()) ? 0 : Transport::connect(host,port);
    }
    int connect(const char* host, uint16_t port, int32_t timeout) override {
        return expired() || !budget.canRequest(Clock::now()) ? 0 : Transport::connect(host,port,timeout);
    }
    int available() override { return expired() ? 0 : Transport::available(); }
    uint8_t connected() override { return expired() ? 0 : Transport::connected(); }
    int read() override { return expired() ? -1 : Transport::read(); }
    int read(uint8_t* data, size_t size) override { return expired() ? -1 : Transport::read(data,size); }
    int peek() override { return expired() ? -1 : Transport::peek(); }
    size_t write(uint8_t data) override { return expired() ? 0 : Transport::write(data); }
    size_t write(const uint8_t* data, size_t size) override { return expired() ? 0 : Transport::write(data,size); }
};
