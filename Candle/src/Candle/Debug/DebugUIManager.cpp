#include "cdlpch.h"
#include "DebugUIManager.h"

#include "Panels/FrameStatsPanel.h"

namespace Candle {

	DebugUIManager::DebugUIManager()
	{
		AddPanel<FrameStatsPanel>();
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
