#include "cdlpch.h"
#include "InputSystem.h"

#include "Candle/Platform/PlatformEvents.h"

namespace Candle {

	void InputSystem::ConsumeEvent(const PlatformEvent& event)
	{
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
			},
			[](const auto&) {}	// Window events aren't input
			}, event);
	}

	const InputSnapshot& InputSystem::CaptureSnapshot()
	{
		m_Published = std::move(m_Building);
		
		// Reset the building snapshot for the next frame
		m_Building.Reset();

		return m_Published;
	}

}