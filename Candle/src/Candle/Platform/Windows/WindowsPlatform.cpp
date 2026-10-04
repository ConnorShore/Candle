#include "cdlpch.h"
#include "Candle/Platform/Platform.h"

#ifndef CDL_PLATFORM_WINDOWS
	#error "WindowsPlatform.cpp is Windows-only."
#endif

#include <Windows.h>
#include <SDL3/SDL.h>

namespace Candle {

	void Platform::Init()
	{
		s_MainThreadId = Platform::GetCurrentThreadId();

		// Time first, so anything initialised after this point can timestamp its own startup.
		InitTime();

		// Init SDL3
		SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD);
	}

	void Platform::Shutdown()
	{
		// Shutdown SDL3
		SDL_Quit();
	}

}
