#include "cdlpch.h"
#include "SDLPlatform.h"
#include "Candle/Platform/Platform.h"

#include <SDL3/SDL_vulkan.h>

namespace Candle {

	namespace {

		SDL::RawEventHook s_RawEventHook = nullptr;     // Main thread only, so plain statics need no ordering
		void* s_RawEventHookData = nullptr;
		bool s_Pumping = false;                         // Catches a hook that tries to change the hook

		KeyCode ToKeyCode(SDL_Scancode scancode)
		{
			if (scancode < 0 || scancode >= std::to_underlying(KeyCode::Count))
				return KeyCode::Unknown;
			return static_cast<KeyCode>(scancode);
		}

		MouseButton ToMouseButton(uint8_t sdlButton)
		{
			if (sdlButton < 1 || sdlButton >= std::to_underlying(MouseButton::Count))
				return MouseButton::Unknown;
			return static_cast<MouseButton>(sdlButton);
		}

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
		CDL_CORE_ASSERT(IsMainThread(), "SDL video must be queried on the main thread");
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
			case SDL_EVENT_KEY_DOWN:
				if (!e.key.repeat)	// Auto-repeat is for text entry and UI, not a fresh gameplay press
					outEvents.emplace_back(KeyPressedEvent{ .Key = ToKeyCode(e.key.scancode) });
				break;
			case SDL_EVENT_KEY_UP:
				outEvents.emplace_back(KeyReleasedEvent{ .Key = ToKeyCode(e.key.scancode) });
				break;
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
				outEvents.emplace_back(MouseButtonPressedEvent{ .Button = ToMouseButton(e.button.button) });
				break;
			case SDL_EVENT_MOUSE_BUTTON_UP:
				outEvents.emplace_back(MouseButtonReleasedEvent{ .Button = ToMouseButton(e.button.button) });
				break;
			case SDL_EVENT_MOUSE_MOTION:
				outEvents.emplace_back(MouseMoveEvent{ .Position = glm::vec2(e.motion.x, e.motion.y), .Delta = glm::vec2(e.motion.xrel, e.motion.yrel) });
				break;
			case SDL_EVENT_MOUSE_WHEEL:
				outEvents.emplace_back(MouseWheelEvent{ .Delta = glm::vec2(e.wheel.x, e.wheel.y), .Ticks = glm::ivec2(e.wheel.integer_x, e.wheel.integer_y) });
				break;
			}
		}

		s_Pumping = false;
	}

	const char* const* Platform::GetVulkanRequiredInstanceExtensions(uint32_t& outCount)
	{
		// SDL dereferences its video device without a null check, so calling this headless would crash.
		CDL_CORE_ASSERT(IsWindowingInitialized(), "Vulkan instance extensions need the windowing backend initialized");
		return SDL_Vulkan_GetInstanceExtensions(&outCount);
	}

}
