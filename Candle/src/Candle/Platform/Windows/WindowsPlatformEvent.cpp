#include "cdlpch.h"
#include "Candle/Platform/Platform.h"

#ifndef CDL_PLATFORM_WINDOWS
#error "WindowsPlatformEvent.cpp is Windows-only."
#endif

#include <SDL3/SDL.h>

namespace Candle {

	void Platform::PumpEvents(std::vector<PlatformEvent>& outEvents)
	{
		CDL_CORE_ASSERT(s_MainThreadId == Platform::GetCurrentThreadId(), "Pumping events must occur on the main thread");

		outEvents.clear();
		
		SDL_Event e;
		while (SDL_PollEvent(&e))
		{
			switch (e.type)
			{
			case SDL_EVENT_QUIT:
				outEvents.emplace_back(QuitRequested{});
				break;
			case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
				outEvents.emplace_back(WindowCloseRequested{ .WindowID = static_cast<uint32_t>(e.window.windowID) });
				break;
			case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
				outEvents.emplace_back(WindowResized{ .Width = static_cast<uint32_t>(e.window.data1), .Height = static_cast<uint32_t>(e.window.data2) });
				break;
			case SDL_EVENT_WINDOW_FOCUS_GAINED:
			case SDL_EVENT_WINDOW_FOCUS_LOST:
				outEvents.emplace_back(WindowFocus{ .Fucused = e.type == SDL_EVENT_WINDOW_FOCUS_GAINED });
				break;
			case SDL_EVENT_WINDOW_MINIMIZED:
			case SDL_EVENT_WINDOW_MAXIMIZED:
				outEvents.emplace_back(WindowMinimized{ .Minimized = e.type == SDL_EVENT_WINDOW_MINIMIZED });
				break;
			}
		}
	}

}