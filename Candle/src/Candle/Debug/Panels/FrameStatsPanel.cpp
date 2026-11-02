#include "cdlpch.h"
#include "FrameStatsPanel.h"

#include <imgui.h>

namespace Candle {

	void FrameStatsPanel::OnRender(const DebugContext& context)
	{
		ImGui::Begin("Frame Stats", &IsOpen);

		ImGui::Text("Delta Time: %.3f ms", context.DeltaTime * 1000.0f);

		ImGui::Text("Main Thread: %.3f ms (Pump Wait: %.3f ms)", context.Main.TotalMs, context.Main.PumpWaitMs);
		ImGui::Text("Render Thread: %.3f ms (Pump Wait: %.3f ms)", context.Render.Timings.TotalMs, context.Render.Timings.PumpWaitMs);

		ImGui::End();
	}

}