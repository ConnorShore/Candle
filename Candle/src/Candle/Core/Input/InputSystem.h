#pragma once

#include "InputCodes.h"
#include "Candle/Platform/PlatformEvents.h"

#include <bitset>
#include <utility>

#include <glm/glm.hpp>

namespace Candle {

	// Captures the state of all input devices at a single point in time. Updated once per frame by InputSystem.
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

		void ConsumeEvent(const PlatformEvent& event);

		// Snapshot is updated once per frame; the returned reference is valid until the next call to CaptureSnapshot().
		const InputSnapshot& CaptureSnapshot();		// Overwritten by the next capture; copy it to keep it or to hand it to another thread

	private:
		InputSnapshot m_Building, m_Published;
	};

}