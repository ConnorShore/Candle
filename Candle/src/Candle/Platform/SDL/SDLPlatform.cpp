#include "cdlpch.h"
#include "SDLPlatform.h"
#include "Candle/Platform/Platform.h"


namespace Candle {

	namespace {

		SDL::RawEventHook s_RawEventHook = nullptr;     // Main thread only, so plain statics need no ordering
		void* s_RawEventHookData = nullptr;
		bool s_Pumping = false;                         // Catches a hook that tries to change the hook

	}

	void SDL::SetRawEventHook(RawEventHook hook, void* userData)
	{
		CDL_CORE_ASSERT(Platform::IsMainThread(), "The raw event hook is main thread only");
		CDL_CORE_ASSERT(!s_Pumping, "The raw event hook cannot change during PumpEvents");
		CDL_CORE_ASSERT(hook == nullptr || s_RawEventHook == nullptr, "A raw event hook is already set");
		s_RawEventHook = hook;
		s_RawEventHookData = userData;
	}

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
		s_Pumping = true;
		while (SDL_PollEvent(&e))
		{
			if (s_RawEventHook)
				s_RawEventHook(e, s_RawEventHookData);

			switch (e.type)
			{
			case SDL_EVENT_QUIT:
				outEvents.emplace_back(QuitRequested{});
				break;
			case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
				outEvents.emplace_back(WindowCloseRequested{ .WindowID = static_cast<uint32_t>(e.window.windowID) });
				break;
			case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
				outEvents.emplace_back(WindowResized{ .WindowID = static_cast<uint32_t>(e.window.windowID), .Width = static_cast<uint32_t>(e.window.data1), .Height = static_cast<uint32_t>(e.window.data2) });
				break;
			case SDL_EVENT_WINDOW_FOCUS_GAINED:
			case SDL_EVENT_WINDOW_FOCUS_LOST:
				outEvents.emplace_back(WindowFocus{ .WindowID = static_cast<uint32_t>(e.window.windowID), .Focused =e.type == SDL_EVENT_WINDOW_FOCUS_GAINED });
				break;
			case SDL_EVENT_WINDOW_MINIMIZED:
			case SDL_EVENT_WINDOW_RESTORED:
				outEvents.emplace_back(WindowMinimized{ .WindowID = static_cast<uint32_t>(e.window.windowID), .Minimized = e.type == SDL_EVENT_WINDOW_MINIMIZED });
				break;
			}
		}

		s_Pumping = false;
	}

}
