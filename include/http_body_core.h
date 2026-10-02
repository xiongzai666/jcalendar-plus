#pragma once
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include "network_budget_core.h"

// Transport-independent HTTP/1 body reader. Check the shared deadline even
// while the peer is connected but available()==0; Stream timeouts alone don't.
template<class Socket, class Clock, class Append>
bool readHttpBody(Socket& socket, Clock& clock, const NetworkBudget& budget,
                  int contentLength, bool chunked, size_t limit, uint32_t idleLimit,
                  Append append) {
    size_t total = 0;
    uint32_t lastData = clock.now();
    const auto wait = [&]() -> bool {
        while (socket.available() <= 0) {
            if (!budget.remaining(clock.now()) || clock.now() - lastData >= idleLimit ||
                !socket.connected()) return false;
            clock.pause();
        }
        return budget.remaining(clock.now()) > 0;
    };
    const auto byte = [&](uint8_t& out) -> bool {
        if (!wait() || socket.read(&out,1) != 1) return false;
        lastData = clock.now(); return true;
    };
    const auto line = [&](char* target, size_t capacity) -> bool {
        size_t count = 0;
        while (count + 1 < capacity) {
            uint8_t c;
            if (!byte(c)) return false;
            if (c == '\r') {
                if (!byte(c) || c != '\n') return false;
                target[count] = 0; return true;
            }
            if (c == '\n' || c == 0) return false;
            target[count++] = c;
        }
        return false;
    };
    const auto readCount = [&](size_t count) -> bool {
        if (!budget.acceptBody(clock.now(),total,count,limit)) return false;
        uint8_t buffer[512];
        while (count) {
            if (!wait()) return false;
            size_t size = socket.available();
            if (size > sizeof(buffer)) size = sizeof(buffer);
            if (size > count) size = count;
            const int got = socket.read(buffer,size);
            if (got <= 0 || !append(buffer,size_t(got))) return false;
            lastData = clock.now(); count -= got; total += got;
        }
        return true;
    };
    if (chunked) {
        char header[128];
        while (true) {
            if (!line(header,sizeof(header))) return false;
            if (char* extension = strchr(header,';')) *extension = 0;
            const size_t digits = strlen(header);
            if (!digits || digits > 8) return false;
            for (size_t i = 0; i < digits; ++i)
                if (!((header[i] >= '0' && header[i] <= '9') || (header[i] >= 'a' && header[i] <= 'f') ||
                      (header[i] >= 'A' && header[i] <= 'F'))) return false;
            const unsigned long count = strtoul(header,nullptr,16);
            if (!count) {
                do { if (!line(header,sizeof(header))) return false; } while (*header);
                return true;
            }
            if (!readCount(count) || !line(header,sizeof(header)) || *header) return false;
        }
    }
    if (contentLength >= 0) return readCount(size_t(contentLength));
    while (socket.connected() || socket.available() > 0) {
        if (!wait()) return !socket.connected() && total > 0 && budget.remaining(clock.now());
        const size_t available = socket.available();
        if (!readCount(available)) return false;
    }
    return total > 0;
}
