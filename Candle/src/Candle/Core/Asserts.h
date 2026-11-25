#pragma once

#include <cstdlib>

#define CDL_STATIC_ASSERT(...) static_assert(__VA_ARGS__)

#ifdef CDL_ENABLE_ASSERTS

// The message must be a string literal: it's joined onto the prefix to become the format string for the arguments after it.
// Pushed past Logger::Enabled under no channel, so a channel mask can never hide why it broke; the logger type says engine or app.
#define CDL_ASSERT_IMPL(loggerFn, x, ...) do { if (!(x)) { ::Candle::Logger::loggerFn()->PushLog(::Candle::LogLevel::Fatal, ::Candle::LogChannel::None, "Assertion Failed: " __VA_ARGS__); __debugbreak(); } } while (false)
#define CDL_CORE_ASSERT(x, ...) CDL_ASSERT_IMPL(CoreLogger, x, __VA_ARGS__)
#define CDL_ASSERT(x, ...) CDL_ASSERT_IMPL(ClientLogger, x, __VA_ARGS__)
#define CDL_DEBUG_BREAK() __debugbreak()

#else
#define CDL_CORE_ASSERT(...) ((void)0)
#define CDL_ASSERT(...) ((void)0)
#define CDL_DEBUG_BREAK()
#endif

// For failures nothing can recover from, so it stays on in every configuration
#define CDL_CORE_PANIC(channel, ...) do { CDL_CORE_FATAL(channel, __VA_ARGS__); ::Candle::Logger::Flush(); CDL_DEBUG_BREAK(); std::abort(); } while (false)
#define CDL_PANIC(channel, ...) do { CDL_FATAL(channel, __VA_ARGS__); ::Candle::Logger::Flush(); CDL_DEBUG_BREAK(); std::abort(); } while (false)
