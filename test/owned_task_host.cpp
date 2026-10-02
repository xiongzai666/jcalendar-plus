#include <cassert>
#include <cstdio>
#include "owned_task_core.h"
struct Connection {
    static int live;
    Connection() { ++live; }
    ~Connection() { --live; }
};
int Connection::live = 0;
int main() {
    bool published = false, deleted = false;
    runOwnedTask([]() { Connection tls; assert(Connection::live == 1); return 2; },
        [&](int status) { assert(Connection::live == 0 && status == 2); published = true; },
        [&]() { assert(Connection::live == 0 && published); deleted = true; });
    assert(deleted);
    puts("Task completion: TLS resources released before ready status and nonreturning deletion");
}
