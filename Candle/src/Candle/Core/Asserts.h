#pragma once

#include <cstdlib>

#define CDL_STATIC_ASSERT(...) static_assert(__VA_ARGS__)

#ifdef CDL_ENABLE_ASSERTS

#define CDL_CORE_ASSERT(x, ...) { if(!(x)) { CDL_CORE_FATAL(::Candle::LogChannel::Application, "Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
#define CDL_ASSERT(x, ...) { if(!(x)) { CDL_FATAL(::Candle::LogChannel::Application, "Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
#define CDL_DEBUG_BREAK() __debugbreak()

#else
#define CDL_CORE_ASSERT(...)
#define CDL_ASSERT(...)
#define CDL_DEBUG_BREAK()
#endif

// For failures nothing can recover from, so it stays on in every configuration
#define CDL_CORE_PANIC(channel, ...) do { CDL_CORE_FATAL(channel, __VA_ARGS__); ::Candle::Logger::Flush(); CDL_DEBUG_BREAK(); std::abort(); } while (false)
#define CDL_PANIC(channel, ...) do { CDL_FATAL(channel, __VA_ARGS__); ::Candle::Logger::Flush(); CDL_DEBUG_BREAK(); std::abort(); } while (false)
