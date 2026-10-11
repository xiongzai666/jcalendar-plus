#pragma once
#include <cstddef>
#include <cstdint>
inline uint32_t& testMillis() { static uint32_t value=0;return value; }
inline uint32_t millis() { return testMillis(); }
inline void delay(int value) { testMillis()+=value; }
struct TestEsp { int restarts=0; void restart() { ++restarts; } };
extern TestEsp ESP;
