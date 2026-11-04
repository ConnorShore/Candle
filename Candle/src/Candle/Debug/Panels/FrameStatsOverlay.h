#pragma once

#include "Candle/Debug/DebugPanel.h"

namespace Candle {

	// TODO:
	//  * Add param for level of info to show in the overlay
	//		* 1 being least info to 3 being most info

	class FrameStatsOverlay : public DebugPanel
	{
	public:
		FrameStatsOverlay() = default;
		~FrameStatsOverlay() override = default;

		virtual void OnUpdate(const DebugContext& context) override;
		virtual void OnRender(const DebugContext& context) override;

		inline virtual const char* GetName() const override { return "Frame Stats Overlay"; }
		inline virtual DebugPanelCategory GetCategory() const override { return DebugPanelCategory::General; }
		inline virtual DebugPanelCommand GetCommand() override
		{
			return DebugPanelCommand{
				.Name{ "Frame" },
				.Description{ "FrameStatsOverlay timing statistics overlay" },
				.Usage{ "Frame" },
				.Callback = [this](ConsoleArgs&& args) {
					if (args.GetCount() == 0)
					{
						IsOpen = !IsOpen;
						return;
					}

					IsOpen = true;
					CDL_CORE_INFO(LogChannel::Command, "FrameStatsOverlay command received with {} arguments. First arg: {}", args.GetCount(), args.TryGetString(0).value_or(""));
				}
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