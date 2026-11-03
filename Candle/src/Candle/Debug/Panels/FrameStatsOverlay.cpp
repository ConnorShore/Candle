#include "cdlpch.h"
#include "FrameStatsOverlay.h"

#include <imgui.h>

#include <cstdio>

namespace Candle {

	namespace {

		constexpr float kRefreshIntervalMs = 500.0f;		// Long enough for the numbers to be read before they change
		constexpr float kTargetMs = 1000.0f / 60.0f;		// Green at or under 60 FPS
		constexpr float kUnacceptableMs = 1000.0f / 30.0f;	// Red past 30 FPS, yellow between

		constexpr float kEdgePadding = 10.0f;
		constexpr float kColumnSpacing = 8.0f;

		constexpr ImU32 kLabelColor = IM_COL32(255, 255, 255, 255);
		constexpr ImU32 kShadowColor = IM_COL32(0, 0, 0, 200);
		constexpr ImU32 kGoodColor = IM_COL32(100, 230, 100, 255);
		constexpr ImU32 kWarnColor = IM_COL32(230, 200, 80, 255);
		constexpr ImU32 kBadColor = IM_COL32(240, 90, 90, 255);

		ImU32 BudgetColor(float ms)
		{
			if (ms <= kTargetMs)
				return kGoodColor;
			if (ms <= kUnacceptableMs)
				return kWarnColor;
			return kBadColor;
		}

		// There's no window behind the text, so a drop shadow keeps it readable over bright scenes
		void DrawShadowedText(ImDrawList* drawList, const ImVec2& pos, ImU32 color, const char* text)
		{
			drawList->AddText({ pos.x + 1.0f, pos.y + 1.0f }, kShadowColor, text);
			drawList->AddText(pos, color, text);
		}

	}

	void FrameStatsOverlay::OnUpdate(const DebugContext& context)
	{
		// Averaged over a window and published all at once, so the numbers hold still long enough to read
		m_Accumulated.FrameMs += context.DeltaTime.GetMilliseconds();
		m_Accumulated.GameMs += context.Main.BusyMs();	// The main thread is the game thread; this is all of its work, not just OnUpdate
		m_Accumulated.RenderMs += context.Render.Timings.BusyMs();
		++m_AccumulatedFrames;

		if (m_Accumulated.FrameMs < kRefreshIntervalMs)
			return;

		const float frames = static_cast<float>(m_AccumulatedFrames);
		m_Displayed = {
			.FrameMs = m_Accumulated.FrameMs / frames,
			.GameMs = m_Accumulated.GameMs / frames,
			.RenderMs = m_Accumulated.RenderMs / frames
		};

		m_Accumulated = {};
		m_AccumulatedFrames = 0;
	}

	void FrameStatsOverlay::OnRender(const DebugContext& context)
	{
		// Nothing averaged yet
		if (m_Displayed.FrameMs <= 0.0f)
			return;

		// The foreground list rather than a window: it never takes input or focus, and stays on top of the
		// "Scene" window when the world isn't rendered straight to the swapchain
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImDrawList* drawList = ImGui::GetForegroundDrawList(viewport);

		// Numbers are right-aligned against their suffix so the decimal points line up in any font
		const float right = viewport->WorkPos.x + viewport->WorkSize.x - kEdgePadding;
		const float numberRight = right - ImGui::CalcTextSize(" FPS").x;
		const float labelRight = numberRight - ImGui::CalcTextSize("000.00").x - kColumnSpacing;
		const float lineHeight = ImGui::GetTextLineHeight() + 1.0f;
		float y = viewport->WorkPos.y + kEdgePadding;

		auto drawRow = [&](const char* label, float value, const char* suffix, ImU32 valueColor)
		{
			char number[16];
			std::snprintf(number, sizeof(number), "%.2f", value);

			DrawShadowedText(drawList, { labelRight - ImGui::CalcTextSize(label).x, y }, kLabelColor, label);
			DrawShadowedText(drawList, { numberRight - ImGui::CalcTextSize(number).x, y }, valueColor, number);
			DrawShadowedText(drawList, { numberRight, y }, valueColor, suffix);
			y += lineHeight;
		};

		drawRow("", 1000.0f / m_Displayed.FrameMs, " FPS", BudgetColor(m_Displayed.FrameMs));
		drawRow("Frame:", m_Displayed.FrameMs, " ms", BudgetColor(m_Displayed.FrameMs));
		drawRow("Game:", m_Displayed.GameMs, " ms", BudgetColor(m_Displayed.GameMs));
		drawRow("Render:", m_Displayed.RenderMs, " ms", BudgetColor(m_Displayed.RenderMs));
		// TODO: Show Draw time and GPU time
		// TODO: Show Memory and VRAM
		// TODO: Show Draws (number of draw calls a frame) and Primitives (number of triangles/primitives a frame)
	}

}
