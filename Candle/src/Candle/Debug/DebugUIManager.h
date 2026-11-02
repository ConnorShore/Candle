#pragma once

#include "DebugPanel.h"
#include "Candle/Core/Memory/ScopedPtr.h"

#include <concepts>
#include <utility>
#include <vector>
#include <concepts>

namespace Candle {

	template<typename T>
	concept IsDebugPanel = std::derived_from<T, DebugPanel>;

	// Owns the debug panels and drives them each frame.
	// Threading: main thread only; OnRender must run between GuiSystem::StartFrame and EndFrame.
	class DebugUIManager
	{
	public:
		DebugUIManager();	// Registers the engine's panels
		~DebugUIManager() = default;

		// Delete copy and move constructors and assignment operators
		DebugUIManager(const DebugUIManager&) = delete;
		DebugUIManager(DebugUIManager&&) = delete;
		DebugUIManager& operator=(const DebugUIManager&) = delete;
		DebugUIManager& operator=(DebugUIManager&&) = delete;

		// Panels are heap-allocated and never move, so the reference stays valid for the manager's lifetime
		template<IsDebugPanel T, typename... Args>
		T& AddPanel(Args&&... args)
		{
			ScopedPtr<T> panel = ScopedPtr<T>::Create(std::forward<Args>(args)...);
			T* raw = panel.Get();
			m_Panels.emplace_back(std::move(panel));
			return *raw;
		}

		void OnUpdate(const DebugContext& context);	// Every panel, open or not
		void OnRender(const DebugContext& context);	// Open panels only, and nothing while hidden

		inline void SetVisible(bool visible) { m_IsVisible = visible; }
		inline bool IsVisible() const { return m_IsVisible; }

	private:
		std::vector<ScopedPtr<DebugPanel>> m_Panels;
		bool m_IsVisible{ true };
	};

}
