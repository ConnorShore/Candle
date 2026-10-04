#pragma once

#include <variant>
#include <cstdint>

namespace Candle {

	struct QuitRequested {};
	struct WindowCloseRequested { uint32_t WindowID; };
	struct WindowResized { uint32_t Width, Height; };
	struct WindowFocus { bool Focused; };
	struct WindowMinimized { bool Minimized; };

	using PlatformEvent = std::variant<QuitRequested, WindowCloseRequested, WindowResized, WindowFocus, WindowMinimized>;

}