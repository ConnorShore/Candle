#include "cdlpch.h"
#include "Candle/Platform/Platform.h"

#include <SDL3/SDL.h>

namespace Candle {

	void Platform::InitWindowing()
	{
		CDL_CORE_ASSERT(IsMainThread(), "SDL video must be initialized on the main thread");

		if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
		{
			const std::string error = std::format("SDL_Init failed: {}", SDL_GetError());
			CDL_CORE_ERROR(LogChannel::Window, "{}", error);
			throw std::runtime_error(error);
		}
	}

	void Platform::ShutdownWindowing()
	{
		SDL_Quit();
	}

	bool Platform::IsWindowingInitialized()
	{
		return SDL_WasInit(SDL_INIT_VIDEO) != 0;
	}

	void Platform::PumpEvents(std::vector<PlatformEvent>& outEvents)
	{
		CDL_CORE_ASSERT(IsMainThread(), "Pumping events must occur on the main thread");
		CDL_CORE_ASSERT(SDL_WasInit(SDL_INIT_EVENTS) != 0, "Without SDL's event queue, polling silently returns nothing; call InitWindowing first");

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
				outEvents.emplace_back(WindowFocus{ .Focused =e.type == SDL_EVENT_WINDOW_FOCUS_GAINED });
				break;
			case SDL_EVENT_WINDOW_MINIMIZED:
			case SDL_EVENT_WINDOW_RESTORED:
				outEvents.emplace_back(WindowMinimized{ .Minimized = e.type == SDL_EVENT_WINDOW_MINIMIZED });
				break;
			}
		}
	}

}
