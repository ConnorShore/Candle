#pragma once

#include "Candle/Debug/DebugPanel.h"

namespace Candle {

	class FrameStatsOverlay : public DebugPanel
	{
	public:
		FrameStatsOverlay() = default;
		~FrameStatsOverlay() override = default;

		virtual void OnUpdate(const DebugContext& context) override;
		virtual void OnRender(const DebugContext& context) override;

		inline virtual const char* GetName() const override { return "Frame Stats Overlay"; }
		inline virtual DebugPanelCategory GetCategory() const override { return DebugPanelCategory::General; }
		inline virtual DebugPanelCommand GetCommand() const override
		{
			return DebugPanelCommand{
				.Name{ "frame" },
				.Description{ "Frame timing statistics overlay" },
				.Usage{ "frame" }
			};
		}

	private:
		struct UnitTimes
		{
			float FrameMs{ 0.0f };
			float GameMs{ 0.0f };
			float RenderMs{ 0.0f };
		};

		UnitTimes m_Accumulated;			// Sums since the last refresh; FrameMs doubles as the window's elapsed time
		UnitTimes m_Displayed;				// The last window's averages, which is what gets drawn
		uint32_t m_AccumulatedFrames{ 0 };
	};

}