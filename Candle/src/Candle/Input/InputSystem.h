#pragma once

#include "InputSnapshot.h"

#include "Candle/Core/Asserts.h"
#include "Candle/Platform/PlatformEvents.h"

namespace Candle {

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