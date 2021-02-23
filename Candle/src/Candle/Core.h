#pragma once

#include <memory>

#ifdef CANDLE_PLATFORM_WINDOWS
#if CANDLE_DYNAMIC_LINK
	#ifdef CANDLE_BUILD_DLL
		#define CANDLE_API __declspec(dllexport)
	#else
		#define CANDLE_API __declspec(dllimport)
	#endif
#else
	#define CANDLE_API
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

#define CANDLE_BIND_EVENT_FN(func) std::bind(&func, this, std::placeholders::_1)

namespace Candle {

	template<typename T>
	using Scope = std::unique_ptr<T>;

	template<typename T>
	using Ref = std::shared_ptr<T>;

}