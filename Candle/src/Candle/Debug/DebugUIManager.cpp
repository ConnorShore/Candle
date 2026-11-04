#include "cdlpch.h"
#include "DebugUIManager.h"
#include "ConsoleArgs.h"

#include "Panels/FrameStatsOverlay.h"
#include "Panels/FrameGraphPanel.h"
#include "Panels/DebugConsolePanel.h"
#include "Panels/TextureInspectorPanel.h"

#include "Candle/Debug/ConsoleManager.h"
#include "Candle/Core/MainThreadLogSink.h"

namespace Candle {

	DebugUIManager::DebugUIManager(ConsoleManager& consoleManager, MainThreadLogSink& mainThreadLogSink, const TexturePool& texturePool)
		: m_ConsoleManager(consoleManager)
	{
		AddPanel<FrameStatsOverlay>();
		AddPanel<FrameGraphPanel>();
		AddPanel<DebugConsolePanel>(consoleManager, mainThreadLogSink);
		AddPanel<TextureInspectorPanel>(texturePool);

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

		ConsoleCommandSpec debugCommandSpec{
			.Name{ kDebugUICommand },
			.Description{ "Toggles the visibility of a debug panel" },
			.Usage{ kDebugUICommandUsage },
			.Callback{ [this](ConsoleArgs&& args) {
				if (args.GetCount() == 0)
				{
					CDL_CORE_WARN(LogChannel::Command, "No panel specified. Usage: {}", kDebugUICommandUsage);
					return;
				}
				if (args.GetCount() > 1)
				{
					CDL_CORE_WARN(LogChannel::Command, "Command '{}' takes 1 argument. Usage: {}", kDebugUICommand, kDebugUICommandUsage);
					return;
				}
				// Toggle the panel's open state
				for (ScopedPtr<DebugPanel>& panel : m_Panels)
				{
					if (args.Matches(0, panel->GetCommand().Name))
					{
						panel->IsOpen = !panel->IsOpen;
						CDL_CORE_INFO(LogChannel::Command, "Panel '{}' is now {}", panel->GetName(), panel->IsOpen ? "open" : "closed");
						return;
					}
				}
				CDL_CORE_WARN(LogChannel::Command, "No panel found for command '{} {}'", kDebugUICommand, args.GetString(0).value_or(""));
			} }
		};

		[[maybe_unused]] ConsoleCommandHandle handle = m_ConsoleManager.RegisterCommand(debugCommandSpec);
	}

}
