#include "cdlpch.h"
#include "DebugUIManager.h"
#include "ConsoleArgs.h"

#include "Panels/FrameStatsOverlay.h"
#include "Panels/FrameGraphPanel.h"
#include "Panels/DebugConsolePanel.h"
#include "Panels/TextureInspectorPanel.h"
#include "Panels/GPUResourceInspectorPanel.h"

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
		AddPanel<GPUResourceInspectorPanel>();
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
		for (auto& [panel, cmdHandle] : m_Panels)
		{
			panel->OnUpdate(context);
		}
	}

	void DebugUIManager::OnRender(const DebugContext& context)
	{
		if (!m_IsVisible)
			return;

		for (auto& [panel, cmdHandle] : m_Panels)
		{
			if (panel->IsOpen)
				panel->OnRender(context);
		}
	}

	ScopedCommandHandle DebugUIManager::RegisterPanelCommand(DebugPanel* panel)
	{
		DebugPanelCommand command = panel->GetCommand();
		ConsoleCommandCallback defaultCallbackFunc([this, panel](ConsoleArgs&& args) {
			panel->IsOpen = !panel->IsOpen;
			});

		ScopedCommandHandle handle = m_ConsoleManager.RegisterCommand({
			.CommandParts = { std::string(kDebugUICommand), command.Name},
			.Description{ command.Description },
			.Usage{ command.Usage },
			.Callback{ command.Callback != nullptr ? command.Callback : defaultCallbackFunc }
			});

		return std::move(handle);
	}

}
