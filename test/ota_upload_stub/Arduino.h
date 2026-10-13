#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
inline uint32_t& testMillis() { static uint32_t value=0;return value; }
inline uint32_t millis() { return testMillis(); }
inline std::function<void()>& testDelayHook() { static std::function<void()> hook;return hook; }
inline void delay(int value) { testMillis()+=value;if(testDelayHook())testDelayHook()(); }
struct TestEsp { int restarts=0; void restart() { ++restarts; } };
extern TestEsp ESP;
