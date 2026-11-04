#pragma once

#include "DebugPanel.h"
#include "Candle/Core/Memory/ScopedPtr.h"
#include "Candle/Platform/PlatformEvents.h"

#include <concepts>
#include <utility>
#include <vector>
#include <concepts>

namespace Candle {

	class ConsoleManager;
	class MainThreadLogSink;
	class TexturePool;

	template<typename T>
	concept IsDebugPanel = std::derived_from<T, DebugPanel>;

	// Owns the debug panels and drives them each frame.
	// Threading: main thread only; OnRender must run between GuiSystem::StartFrame and EndFrame.
	class DebugUIManager
	{
	public:
		DebugUIManager(ConsoleManager& consoleManager, MainThreadLogSink& mainThreadLogSink, const TexturePool& texturePool);	// Registers the engine's panels
		~DebugUIManager() = default;

		// Delete copy and move constructors and assignment operators
		DebugUIManager(const DebugUIManager&) = delete;
		DebugUIManager(DebugUIManager&&) = delete;
		DebugUIManager& operator=(const DebugUIManager&) = delete;
		DebugUIManager& operator=(DebugUIManager&&) = delete;

		template<IsDebugPanel T, typename... Args>
		void AddPanel(Args&&... args)
		{
			CDL_CORE_ASSERT(m_RegisteredPanelCommands == false, "Cannot add panels after registering panel commands.");

			ScopedPtr<T> panel = ScopedPtr<T>::Create(std::forward<Args>(args)...);
			ScopedCommandHandle cmdHandle = RegisterPanelCommand(panel.Get());
			m_Panels.emplace_back(std::move(panel), std::move(cmdHandle));
		}

		void ConsumeEvent(const InputEvent& event);

		void OnUpdate(const DebugContext& context);	// Every panel, open or not
		void OnRender(const DebugContext& context);	// Open panels only, and nothing while hidden

		inline void SetVisible(bool visible) { m_IsVisible = visible; }
		inline bool IsVisible() const { return m_IsVisible; }

	private:
		inline static constexpr std::string_view kDebugUICommand = "Debug";					// All debug ui commands start with this prefix, followed by the panel's command name
		inline static constexpr std::string_view kDebugUICommandUsage = "Debug.<panel>";	// Usage for the debug command, which is the same for all panels

	private:
		ScopedCommandHandle RegisterPanelCommand(DebugPanel* panel);

		struct PanelEntry
		{
			ScopedPtr<DebugPanel> Panel;
			ScopedCommandHandle CmdHandle;
		};

	private:
		ConsoleManager& m_ConsoleManager;
		std::vector<PanelEntry> m_Panels;
		bool m_IsVisible{ true };
		bool m_RegisteredPanelCommands{ false };
	};

}
