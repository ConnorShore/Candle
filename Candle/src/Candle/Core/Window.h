#pragma once

#include "WindowSpecification.h"

#include <string>
#include <cstdint>

#include <glm/glm.hpp>

namespace Candle {

	using NativeWindowHandle = uintptr_t;

	// Threading: main thread only. Other threads learn of size/minimize changes from PlatformEvents.
	class Window
	{
	public:
		explicit Window(const WindowSpecification& info);
		~Window();

		// Neither copyable nor movable
		Window(const Window&) = delete;
		Window& operator=(const Window&) = delete;

		void Resize(uint32_t width, uint32_t height);

		void Minimize();
		void Maximize();
		void Restore();

		void Destroy();

		glm::uvec2 GetSize() const;
		uint32_t GetID() const;

		inline uint32_t GetWidth() const { return GetSize().x; }
		inline uint32_t GetHeight() const { return GetSize().y; }

	private:
		NativeWindowHandle m_NativeHandle;
	};

}