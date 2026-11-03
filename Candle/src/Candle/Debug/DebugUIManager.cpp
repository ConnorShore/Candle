#include "cdlpch.h"
#include "DebugUIManager.h"

#include "Panels/FrameStatsPanel.h"
#include "Panels/DebugConsolePanel.h"

#include "Candle/Debug/ConsoleManager.h"
#include "Candle/Core/MainThreadLogSink.h"

namespace Candle {

	DebugUIManager::DebugUIManager(ConsoleManager& consoleManager, MainThreadLogSink& mainThreadLogSink)
		: m_ConsoleManager(consoleManager)
	{
		AddPanel<FrameStatsPanel>();
		AddPanel<DebugConsolePanel>(consoleManager, mainThreadLogSink);

		RegisterPanelCommands();
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

	void DebugUIManager::RegisterPanelCommands()
	{
		CDL_CORE_ASSERT(m_RegisteredPanelCommands == false, "Panel commands have already been registered.");
		m_RegisteredPanelCommands = true;

		m_ConsoleManager.RegisterCommand(kDebugUICommand, [this](const std::vector<std::string>& args) {
			if (args.empty())
			{
				CDL_CORE_WARN(LogChannel::Command, "No panel specified. Usage: {}", kDebugUICommandUsage);
				return;
			}

			if (args.size() > 1)
			{
				CDL_CORE_WARN(LogChannel::Command, "Command '{}' takes 1 argument. Usage: {}", kDebugUICommand, kDebugUICommandUsage);
				return;
			}

			// Toggle the panel's open state
			std::string_view panelCommand = args[0];
			for (ScopedPtr<DebugPanel>& panel : m_Panels)
			{
				if (panel->GetCommand().Name == panelCommand)
				{
					panel->IsOpen = !panel->IsOpen;
					CDL_CORE_INFO(LogChannel::Command, "Panel '{}' is now {}", panel->GetName(), panel->IsOpen ? "open" : "closed");
					return;
				}
			}

			CDL_CORE_WARN(LogChannel::Command, "No panel found for command '{}'", kDebugUICommand);
		});
	}

}
