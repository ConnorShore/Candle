#include "cdlpch.h"
#include "Candle/Platform/Platform.h"
#include "Candle/Core/Logger.h"

#ifndef CDL_PLATFORM_WINDOWS
#error "WindowsPlatformWindow.cpp is Windows-only."
#endif

#include <SDL3/SDL.h>

namespace Candle {

	NativeWindowHandle Platform::CreateWindow(const WindowSpecification& info)
	{
		uint64_t flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN;
		if (info.Mode == WindowMode::Borderless)
			flags |= SDL_WINDOW_BORDERLESS;
		else if (info.Mode == WindowMode::Fullscreen)
			flags |= SDL_WINDOW_FULLSCREEN;

		SDL_Window* sdlWindow = SDL_CreateWindow(info.Title.c_str(), info.Width, info.Height, flags);
		if (sdlWindow == nullptr)
		{
			CDL_CORE_ERROR(LogChannel::Window, "Failed to create SDL window: {}", SDL_GetError());
			return 0;
		}

		return reinterpret_cast<NativeWindowHandle>(sdlWindow);
	}

	void Platform::DestroyWindow(NativeWindowHandle handle)
	{
		SDL_DestroyWindow(reinterpret_cast<SDL_Window*>(handle));
	}

	void Platform::ResizeWindow(NativeWindowHandle handle, uint32_t width, uint32_t height)
	{
		SDL_SetWindowSize(reinterpret_cast<SDL_Window*>(handle), width, height);
	}

	void Platform::MinimizeWindow(NativeWindowHandle handle)
	{
		SDL_MinimizeWindow(reinterpret_cast<SDL_Window*>(handle));
	}

	void Platform::MaximizeWindow(NativeWindowHandle handle)
	{
		SDL_MaximizeWindow(reinterpret_cast<SDL_Window*>(handle));
	}

	void Platform::RestoreWindow(NativeWindowHandle handle)
	{
		SDL_RestoreWindow(reinterpret_cast<SDL_Window*>(handle));
	}

}