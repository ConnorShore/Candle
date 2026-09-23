#include "cdlpch.h"
#include "Candle/Platform/Platform.h"

#ifndef CDL_PLATFORM_WINDOWS
	#error "WindowsPlatform.cpp is Windows-only."
#endif

#include <Windows.h>

namespace Candle {

	void Platform::Init()
	{
		// Time first, so anything initialised after this point can timestamp its own startup.
		InitTime();
	}

}
