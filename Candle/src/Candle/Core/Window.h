#pragma once

#include "Candle/Platform/Platform.h"

#include <string>
#include <cstdint>

namespace Candle {

	class Window
	{
	public:
		Window(const WindowSpecification& info);
		~Window();

		Window(Window&& other) noexcept = default;
		Window& operator=(Window&& other) noexcept = default;

		// Delete copy and assignments
		Window(Window&) = delete;
		Window& operator=(Window&) = delete;

		void Resize(uint32_t width, uint32_t height);

		void Minimize();
		void Maximize();
		void Restore();

		void Close();

	private:
		std::string m_Title;
		uint32_t m_Width, m_Height;

		NativeWindowHandle m_NativeHandle;
	};

}