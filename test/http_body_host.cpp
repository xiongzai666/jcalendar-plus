#include <cassert>
#include <string>
#include <cstdio>
#include <cstring>
#include "http_body_core.h"
struct Clock {
    uint32_t stamp = 0;
    uint32_t now() { return stamp; }
    void pause() { ++stamp; }
};
struct Socket {
    std::string data; size_t at = 0; bool staysOpen = false;
    Socket(const char* value, bool open = false) : data(value), staysOpen(open) {}
    int available() { return int(data.size() - at); }
    bool connected() { return staysOpen || at < data.size(); }
    int read(uint8_t* buffer, size_t size) {
        if (size > data.size() - at) size = data.size() - at;
        memcpy(buffer,data.data()+at,size); at += size; return int(size);
    }
};
int main() {
    NetworkBudget budget; budget.begin(0,75000); Clock clock;
    std::string body;
    const auto append = [&body](const uint8_t* bytes,size_t count) { body.append(reinterpret_cast<const char*>(bytes),count); return true; };
    Socket stalled("",true);
    assert(!readHttpBody(stalled,clock,budget,100,false,12000,5000,append));
    assert(clock.stamp == 5000 && body.empty());
    Socket chunked("4\r\ntest\r\n3;foo=bar\r\n123\r\n0\r\nX-Trailer: ok\r\n\r\n");
    assert(readHttpBody(chunked,clock,budget,-1,true,12000,5000,append));
    assert(body == "test123"); body.clear();
    Socket partial("data",true);
    assert(!readHttpBody(partial,clock,budget,100,false,12000,5000,append));
    assert(body == "data"); body.clear();
    Socket oversized("6\r\n123456\r\n0\r\n\r\n");
    assert(!readHttpBody(oversized,clock,budget,-1,true,5,5000,append));
    Socket truncated("4\r\nte");
    assert(!readHttpBody(truncated,clock,budget,-1,true,12000,5000,append));
    puts("HTTP body: open stalled socket, idle timeout, chunked framing, size limit and truncation passed");
}
