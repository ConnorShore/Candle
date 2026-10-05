#pragma once

#include "InputCodes.h"
#include "Candle/Platform/PlatformEvents.h"

#include <bitset>
#include <utility>

#include <glm/glm.hpp>

namespace Candle {

	// The state of all input devices at one capture; edges and deltas cover everything since the previous capture.
	struct InputSnapshot
	{
		using KeyBits = std::bitset<std::to_underlying(KeyCode::Count)>;    // One cache line (64 bytes)
		KeyBits KeysDown;													// Held at end of frame
		KeyBits KeysPressed;												// Went down this frame (even if released in the same frame)
		KeyBits KeysReleased;												// Went up this frame (even if pressed in the same frame)

		uint8_t MouseDown = 0, MousePressed = 0, MouseReleased = 0;		// Bit (button - 1), SDL_BUTTON_MASK's layout
		glm::vec2 MousePosition{};										// Window coordinates, not pixels
		glm::vec2 MouseDelta{};
		glm::vec2 WheelDelta{};
		glm::ivec2 WheelTicks{};

		bool IsKeyDown(KeyCode key) const { return KeysDown[std::to_underlying(key)]; }
		bool IsKeyPressed(KeyCode key) const { return KeysPressed[std::to_underlying(key)]; }
		bool IsKeyReleased(KeyCode key) const { return KeysReleased[std::to_underlying(key)]; }

		bool IsMouseDown(MouseButton button) const { return MouseDown & ButtonBit(button); }
		bool IsMousePressed(MouseButton button) const { return MousePressed & ButtonBit(button); }
		bool IsMouseReleased(MouseButton button) const { return MouseReleased & ButtonBit(button); }

	private:
		friend class InputSystem;

		static constexpr uint8_t ButtonBit(MouseButton button)
		{
			const MouseButtonType value = std::to_underlying(button);
			return (value == 0 || value >= std::to_underlying(MouseButton::Count)) ? 0 : static_cast<uint8_t>(1u << (value - 1));
		}

		void Reset()
		{
			KeysPressed.reset();
			KeysReleased.reset();
			MousePressed = 0;
			MouseReleased = 0;
			MouseDelta = glm::vec2(0.0f);
			WheelDelta = glm::vec2(0.0f);
			WheelTicks = glm::ivec2(0);
		}
	};

	// A finished snapshot crosses threads by value (frame packet, MPSCRingBuffer); copying one mid-ConsumeEvent is still a race.
	CDL_STATIC_ASSERT(std::is_trivially_copyable_v<InputSnapshot>, "InputSnapshot must be trivially copyable to be handed to another thread by value");

	// Threading: Main thread only
	class InputSystem
	{
	public:
		InputSystem() = default;
		~InputSystem() = default;

		// Delete copy and move constructors and assignment operators
		InputSystem(InputSystem&) = delete;
		InputSystem(InputSystem&&) = delete;
		InputSystem& operator=(InputSystem&) = delete;
		InputSystem& operator=(InputSystem&&) = delete;

		void ConsumeEvent(const InputEvent& event);

		// Call once per consumer step (a sim tick, not a render frame): each edge is reported by exactly one capture.
		InputSnapshot CaptureSnapshot();

	private:
		InputSnapshot m_Building;
	};

}