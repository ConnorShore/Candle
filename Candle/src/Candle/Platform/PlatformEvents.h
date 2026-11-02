#pragma once

#include "Candle/Input/InputCodes.h"

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

	// Nested so Application forwards all input in one handler while InputSystem's visit stays exhaustive over it.
	using InputEvent = std::variant<KeyPressedEvent, KeyReleasedEvent, MouseButtonPressedEvent, MouseButtonReleasedEvent, MouseMoveEvent, MouseWheelEvent>;

	using PlatformEvent = std::variant<QuitRequested, WindowCloseRequested, WindowResized, WindowFocus, WindowMinimized, InputEvent>;

	constexpr bool IsKeyEvent(const InputEvent& event) { return std::holds_alternative<KeyPressedEvent>(event) || std::holds_alternative<KeyReleasedEvent>(event); }
	constexpr bool IsMouseEvent(const InputEvent& event) { return std::holds_alternative<MouseButtonPressedEvent>(event) || std::holds_alternative<MouseButtonReleasedEvent>(event) || std::holds_alternative<MouseMoveEvent>(event) || std::holds_alternative<MouseWheelEvent>(event); }
}