#include "cdlpch.h"
#include "DebugUIManager.h"

#include "Panels/FrameStatsPanel.h"
#include "Panels/DebugConsolePanel.h"

#include "Candle/Debug/ConsoleManager.h"
#include "Candle/Core/MainThreadLogSink.h"

namespace Candle {

	DebugUIManager::DebugUIManager(ConsoleManager& consoleManager, MainThreadLogSink& mainThreadLogSink)
	{
		AddPanel<FrameStatsPanel>();
		AddPanel<DebugConsolePanel>(consoleManager, mainThreadLogSink);
	}

	void DebugUIManager::ConsumeEvent(const InputEvent& event)
	{
		CDL_CORE_ASSERT(Platform::IsMainThread(), "InputSystem::ConsumeEvent() must be called from the main thread.");

		std::visit(Overloaded{
			[this](const KeyPressedEvent& e) {
				if (e.Key == KeyCode::Unknown)
					return;

				// Toggle visibility of the debug UI when F1 is pressed
				if (e.Key == KeyCode::F1)
					m_IsVisible = !m_IsVisible;
			},
			[this](const KeyReleasedEvent& e) { },
			[this](const MouseButtonPressedEvent& e) { },
			[this](const MouseButtonReleasedEvent& e) { },
			[this](const MouseMoveEvent& e) { },
			[this](const MouseWheelEvent& e) { }
			}, event);
	}

	void DebugUIManager::OnUpdate(const DebugContext& context)
	{
		for (ScopedPtr<DebugPanel>& panel : m_Panels)
			panel->OnUpdate(context);
	}

	void DebugUIManager::OnRender(const DebugContext& context)
	{
		if (!m_IsVisible)
			return;

		for (ScopedPtr<DebugPanel>& panel : m_Panels)
		{
			if (panel->IsOpen)
				panel->OnRender(context);
		}
	}

}
