#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cstddef>
#include <string>
#include "deadline_client_core.h"
struct Clock { static uint32_t stamp; static uint32_t now() { return stamp; } };
uint32_t Clock::stamp = 0;
struct Transport {
    bool open = true; uint32_t timeout = 5000; uint32_t nextByte = 4000;
    virtual int available() { return open && Clock::stamp >= nextByte; }
    virtual uint8_t connected() { return open; }
    virtual int read() { if (!available()) return -1; nextByte += 4000; return 'a'; }
    virtual int read(uint8_t* data, size_t size) { int c = read(); if (c < 0 || !size) return 0; *data = c; return 1; }
    virtual int peek() { return available() ? 'a' : -1; }
    virtual size_t write(const uint8_t*, size_t size) { return open ? size : 0; }
    virtual size_t write(uint8_t data) { return write(&data,1); }
    void stop() { open = false; }
    void disableReadWait() { timeout = 0; }
    // Mirrors Stream::timedRead: each new byte restarts the idle timeout.
    int timedRead() {
        uint32_t start = Clock::stamp;
        do { int value = read(); if (value >= 0) return value; ++Clock::stamp; }
        while (Clock::stamp - start < timeout);
        return -1;
    }
};
int main() {
    NetworkBudget budget; budget.begin(0,75000);
    DeadlineClient<Transport,Clock> client(budget);
    std::string header;
    for (int i = 0; i < 30; ++i) {
        int value = client.timedRead(); if (value < 0) break;
        header += char(value);
    }
    assert(Clock::stamp <= 75001 && header.size() == 18);
    assert(!client.connected() && client.timeout == 0 && client.write(uint8_t('x')) == 0);
    puts("HTTP headers: slow trickle stops at shared deadline and Stream wait is disabled");
}
