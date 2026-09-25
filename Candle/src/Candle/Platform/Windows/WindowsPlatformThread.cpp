#include "cdlpch.h"
#include "Candle/Platform/Platform.h"

#ifndef CDL_PLATFORM_WINDOWS
#error "WindowsPlatformThread.cpp is Windows-only."
#endif

#include <Windows.h>

namespace Candle {

	uint32_t Platform::GetCurrentThreadId()
	{
		return static_cast<uint32_t>(::GetCurrentThreadId());
	}

}
