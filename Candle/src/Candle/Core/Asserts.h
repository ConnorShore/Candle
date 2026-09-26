#pragma once

#define CDL_STATIC_ASSERT(...) static_assert(__VA_ARGS__)

#ifdef CDL_ENABLE_ASSERTS

#define CDL_CORE_ASSERT(x, ...) { if(!(x)) { CDL_CORE_FATAL(::Candle::LogChannel::Application, "Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
#define CDL_ASSERT(x, ...) { if(!(x)) { CDL_FATAL(::Candle::LogChannel::Application, "Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }

#else
#define CDL_CORE_ASSERT(...)
#define CDL_ASSERT(...)
#endif
