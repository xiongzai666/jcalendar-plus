#pragma once
#include <cstddef>
#include <cstdint>
inline void delay(int) {}
struct TestEsp { int restarts=0; void restart() { ++restarts; } };
extern TestEsp ESP;
