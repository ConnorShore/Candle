#include "cdlpch.h"
#include "InputSystem.h"

namespace Candle {

	void InputSystem::ConsumeEvent(const InputEvent& event)
	{
		CDL_CORE_ASSERT(Platform::IsMainThread(), "InputSystem::ConsumeEvent() must be called from the main thread.");

		std::visit(Overloaded{
			[this](const KeyPressedEvent& e) {
				if (e.Key == KeyCode::Unknown)
					return;
				m_Building.KeysDown.set(std::to_underlying(e.Key), true);
				m_Building.KeysPressed.set(std::to_underlying(e.Key), true);
			},
			[this](const KeyReleasedEvent& e) {
				if (e.Key == KeyCode::Unknown)
					return;
				m_Building.KeysDown.set(std::to_underlying(e.Key), false);
				m_Building.KeysReleased.set(std::to_underlying(e.Key), true);
			},
			[this](const MouseButtonPressedEvent& e) {
				m_Building.MouseDown |= InputSnapshot::ButtonBit(e.Button);
				m_Building.MousePressed |= InputSnapshot::ButtonBit(e.Button);
			},
			[this](const MouseButtonReleasedEvent& e) {
				m_Building.MouseDown &= ~InputSnapshot::ButtonBit(e.Button);
				m_Building.MouseReleased |= InputSnapshot::ButtonBit(e.Button);
			},
			[this](const MouseMoveEvent& e) {
				m_Building.MouseDelta += e.Delta;
				m_Building.MousePosition = e.Position;
			},
			[this](const MouseWheelEvent& e) {
				m_Building.WheelDelta += e.Delta;
				m_Building.WheelTicks += e.Ticks;
			}
			}, event);
	}

	InputSnapshot InputSystem::CaptureSnapshot()
	{
		CDL_CORE_ASSERT(Platform::IsMainThread(), "InputSystem::CaptureSnapshot() must be called from the main thread.");

		// Copy, not move: held state must survive into the next capture
		InputSnapshot snapshot = m_Building;

		// Clear the edges and deltas for the next capture
		m_Building.Reset();

		return snapshot;
	}

}