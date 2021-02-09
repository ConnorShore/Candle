#pragma once

#ifdef CANDLE_PLATFORM_WINDOWS
	#ifdef CANDLE_BUILD_DLL
		#define CANDLE_API __declspec(dllexport)
	#else
		#define CANDLE_API __declspec(dllimport)
	#endif
#else
	#error Candle only supports Windows!
#endif

#ifdef CANDLE_ENABLE_ASSERTS
	#define CANDLE_CORE_ASSERT(x, ...) { if(!(x)) { CANDLE_CORE_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
	#define CANDLE_ASSERT(x, ...) { if(!(x)) { CANDLE_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
#else
	#define CANDLE_CORE_ASSERT(x, ...)
	#define CANDLE_ASSERT(x, ...)
#endif

#define BIT(x) (1<<x)