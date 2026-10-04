#include "cdlpch.h"
#include "Candle/Core/Window.h"
#include "Candle/Platform/Platform.h"

#include <SDL3/SDL.h>

namespace Candle {

	namespace {

		SDL_Window* ToSDL(NativeWindowHandle handle) 
		{
			CDL_CORE_ASSERT(handle != 0, "Window has been destroyed");
			return reinterpret_cast<SDL_Window*>(handle); 
		}

	}

	Window::Window(const WindowSpecification& info)
	{
		CDL_CORE_ASSERT(Platform::IsMainThread(), "Windows must be created on the main thread");

		uint64_t flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN;
		if (info.Mode == WindowMode::BorderlessFullscreen)
			flags |= SDL_WINDOW_FULLSCREEN;	// With no display mode set, SDL's fullscreen is borderless desktop fullscreen

		SDL_Window* sdlWindow = SDL_CreateWindow(info.Title.c_str(), info.Width, info.Height, flags);
		if (sdlWindow == nullptr)
		{
			CDL_CORE_ERROR(LogChannel::Window, "Failed to create SDL window: {}", SDL_GetError());
			throw std::runtime_error(std::format("Failed to create window '{}'", info.Title));
		}

		m_NativeHandle = reinterpret_cast<NativeWindowHandle>(sdlWindow);
	}

	Window::~Window()
	{
		Destroy();
	}

	void Window::Resize(uint32_t width, uint32_t height)
	{
		CDL_CORE_ASSERT(Platform::IsMainThread(), "Windows must be resized on the main thread");
		SDL_SetWindowSize(ToSDL(m_NativeHandle), width, height);
	}

	void Window::Minimize()
	{
		CDL_CORE_ASSERT(Platform::IsMainThread(), "Windows must be minimized on the main thread");
		SDL_MinimizeWindow(ToSDL(m_NativeHandle));
	}

	void Window::Maximize()
	{
		CDL_CORE_ASSERT(Platform::IsMainThread(), "Windows must be maximized on the main thread");
		SDL_MaximizeWindow(ToSDL(m_NativeHandle));
	}

	void Window::Restore()
	{
		CDL_CORE_ASSERT(Platform::IsMainThread(), "Windows must be restored on the main thread");
		SDL_RestoreWindow(ToSDL(m_NativeHandle));
	}

	void Window::Destroy()
	{
		CDL_CORE_ASSERT(Platform::IsMainThread(), "Windows must be destroyed on the main thread");
		if (m_NativeHandle != 0)
		{
			SDL_DestroyWindow(ToSDL(m_NativeHandle));
			m_NativeHandle = 0;
		}
	}

	glm::uvec2 Window::GetSize() const
	{
		CDL_CORE_ASSERT(Platform::IsMainThread(), "Windows must be queried for size on the main thread");
		int width, height;
		SDL_GetWindowSizeInPixels(ToSDL(m_NativeHandle), &width, &height);
		return glm::uvec2(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
	}

}
