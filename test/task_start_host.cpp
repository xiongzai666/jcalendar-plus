#include <cassert>
#include <cstdio>
#include "task_start_core.h"
int main() {
    void* handle = nullptr; int status = -1; int starts = 0;
    assert(!startTaskIfIdle([&](void** out) { ++starts; *out = nullptr; return false; },handle,status));
    assert(status == 2 && !handle && starts == 1);
    assert(startTaskIfIdle([&](void** out) { ++starts; *out = &starts; return true; },handle,status));
    assert(status == 0 && handle && starts == 2);
    assert(!startTaskIfIdle([&](void**) { ++starts; return false; },handle,status));
    assert(status == 0 && starts == 2);
    assert(weatherStartAllowed(-1,1,false,false));
    assert(!weatherStartAllowed(-1,1,true,false));
    assert(!weatherStartAllowed(-1,1,false,true));
    assert(!weatherStartAllowed(-1,0,false,false));
    assert(!weatherStartAllowed(0,1,false,false));
    puts("task start: terminal failure, retained active task, portal excludes weather");
}
