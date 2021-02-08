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

#define BIT(x) (1<<x)