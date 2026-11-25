#include "cdlpch.h"
#include "Candle/Platform/Platform.h"

#ifndef CDL_PLATFORM_WINDOWS
	#error "WindowsPlatform.cpp is Windows-only."
#endif

#include <Windows.h>

namespace Candle {

	void Platform::Init()
	{
		s_MainThreadId = Platform::GetCurrentThreadId();

		// Time first, so anything initialised after this point can timestamp its own startup.
		InitTime();

		// Here rather than in InitWindowing: SDL frees through whatever functions are set at the time, so they have to be
		// in place before anything (tests included) makes its first SDL call.
		InitWindowingAllocator();
	}

	void Platform::Shutdown()
	{
	}

}
