#pragma once

#include "Candle/Debug/DebugPanel.h"

namespace Candle {

	class FrameStatsPanel : public DebugPanel
	{
	public:
		FrameStatsPanel() = default;
		~FrameStatsPanel() override = default;

		virtual void OnRender(const DebugContext& context) override;

		inline virtual const char* GetName() const override { return "Frame Stats"; }
		inline virtual DebugPanelCategory GetCategory() const override { return DebugPanelCategory::General; }

	};

}