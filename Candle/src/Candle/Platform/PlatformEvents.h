#pragma once

#include "Candle/Core/Input/InputCodes.h"

#include <variant>
#include <cstdint>

#include <glm/glm.hpp>

namespace Candle {

	// Window Events //
	struct QuitRequested {};
	struct WindowCloseRequested { uint32_t WindowID; };
	struct WindowResized { uint32_t WindowID, Width, Height; };
	struct WindowFocus { uint32_t WindowID; bool Focused; };
	struct WindowMinimized { uint32_t WindowID; bool Minimized; };

	// Input Events //
	struct KeyPressedEvent { KeyCode Key; };
	struct KeyReleasedEvent { KeyCode Key; };
	struct MouseButtonPressedEvent { MouseButton Button; };
	struct MouseButtonReleasedEvent { MouseButton Button; };
	struct MouseMoveEvent { glm::vec2 Position; glm::vec2 Delta; };
	struct MouseWheelEvent { glm::vec2 Delta; glm::ivec2 Ticks; };

	using PlatformEvent = std::variant<QuitRequested, WindowCloseRequested, WindowResized, WindowFocus, WindowMinimized,
							KeyPressedEvent, KeyReleasedEvent, MouseButtonPressedEvent, MouseButtonReleasedEvent, MouseMoveEvent, MouseWheelEvent>;

}