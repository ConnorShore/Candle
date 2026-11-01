#include "cdlpch.h"
#include "Candle/Platform/Window.h"
#include "Candle/Platform/Platform.h"
#include "Candle/Render/GUI/GuiSystem.h"

#include <SDL3/SDL.h>

namespace Candle {

	namespace {

		SDL_Window* ToSDL(NativeWindowHandle handle) 
		{
			CDL_CORE_ASSERT(handle != 0, "Window has been destroyed");
			return reinterpret_cast<SDL_Window*>(handle); 
		}

		constexpr bool IsNativeWindowHandleValid(NativeWindowHandle handle) { return handle != 0; }
	}

	Window::Window(const WindowSpecification& info)
	{
		CDL_CORE_ASSERT(Platform::IsMainThread(), "Windows must be created on the main thread");
		CDL_CORE_ASSERT(Platform::IsWindowingInitialized(), "InitWindowing must run before a window is created");

		uint64_t flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
		if (info.Mode == WindowMode::BorderlessFullscreen)
			flags |= SDL_WINDOW_FULLSCREEN;	// With no display mode set, SDL's fullscreen is borderless desktop fullscreen

		float scale = GetContentScale();	// Scale the window size by the content scale to get the correct pixel size for high-DPI displays
		SDL_Window* sdlWindow = SDL_CreateWindow(info.Title.c_str(), static_cast<int>(info.Width * scale), static_cast<int>(info.Height * scale), flags);
		if (sdlWindow == nullptr)
			CDL_CORE_PANIC(LogChannel::Window, "Failed to create window '{}': {}", info.Title, SDL_GetError());

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
		if (IsNativeWindowHandleValid(m_NativeHandle))
		{
			SDL_DestroyWindow(ToSDL(m_NativeHandle));
			m_NativeHandle = 0;
		}
	}

	float Window::GetContentScale() const
	{
		CDL_CORE_ASSERT(Platform::IsMainThread(), "Windows must be queried for content scale on the main thread");
		SDL_DisplayID displayID = IsNativeWindowHandleValid(m_NativeHandle) ? SDL_GetDisplayForWindow(ToSDL(m_NativeHandle)) : SDL_GetPrimaryDisplay();
		return SDL_GetDisplayContentScale(displayID);
	}

	bool Window::IsMinimized() const
	{
		CDL_CORE_ASSERT(Platform::IsMainThread(), "Windows must be queried for minimized state on the main thread");
		return SDL_GetWindowFlags(ToSDL(m_NativeHandle)) & SDL_WINDOW_MINIMIZED;
	}

	glm::uvec2 Window::GetSize() const
	{
		CDL_CORE_ASSERT(Platform::IsMainThread(), "Windows must be queried for size on the main thread");
		int width, height;
		SDL_GetWindowSizeInPixels(ToSDL(m_NativeHandle), &width, &height);
		return glm::uvec2(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
	}

	uint32_t Window::GetID() const
	{
		CDL_CORE_ASSERT(Platform::IsMainThread(), "Windows must be queried for ID on the main thread");
		return static_cast<uint32_t>(SDL_GetWindowID(ToSDL(m_NativeHandle)));
	}

}
