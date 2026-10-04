#pragma once

#include <variant>
#include <cstdint>

namespace Candle {

	struct QuitRequested {};
	struct WindowCloseRequested { uint32_t WindowID; };
	struct WindowResized { uint32_t WindowID, Width, Height; };
	struct WindowFocus { uint32_t WindowID; bool Focused; };
	struct WindowMinimized { uint32_t WindowID; bool Minimized; };

	using PlatformEvent = std::variant<QuitRequested, WindowCloseRequested, WindowResized, WindowFocus, WindowMinimized>;

}