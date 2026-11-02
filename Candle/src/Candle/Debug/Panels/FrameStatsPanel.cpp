#include "cdlpch.h"
#include "FrameStatsPanel.h"

#include <imgui.h>

#include <algorithm>
#include <numeric>
#include <limits>
#include <cmath>
#include <cstdio>

namespace Candle {

	namespace {

		constexpr float kBudgetMs = 1000.0f / 60.0f;	// The 60 FPS line
		constexpr float kDetailsWidthFraction = 0.25f;	// The details pane's share of the window
		constexpr float kMinGraphSize = 50.0f;			// InvisibleButton asserts on a zero size

		constexpr ImU32 kMainColor = IM_COL32(100, 210, 100, 255);
		constexpr ImU32 kRenderColor = IM_COL32(240, 160, 60, 255);
		constexpr ImU32 kHoverColor = IM_COL32(255, 255, 255, 60);
		constexpr ImU32 kSelectionColor = IM_COL32(255, 255, 255, 200);

	}

	// -------------------------------------------------------
	// StatHistory
	// -------------------------------------------------------

	void FrameStatsPanel::StatHistory::Push(float value)
	{
		Values[CurrentIndex] = value;
		CurrentIndex = (CurrentIndex + 1) % kMaxHistorySize;
		Count = std::min<uint32_t>(Count + 1, kMaxHistorySize);
	}

	float FrameStatsPanel::StatHistory::Max()
	{
		return *std::max_element(Values.begin(), Values.begin() + Count);
	}

	float FrameStatsPanel::StatHistory::Min()
	{
		return *std::min_element(Values.begin(), Values.begin() + Count);
	}

	float FrameStatsPanel::StatHistory::Average()
	{
		return std::accumulate(Values.begin(), Values.begin() + Count, 0.0f) / static_cast<float>(Count);
	}

	float FrameStatsPanel::StatHistory::AtAge(uint32_t age) const
	{
		return Values[(CurrentIndex + kMaxHistorySize - 1 - age) % kMaxHistorySize];
	}

	// -------------------------------------------------------
	// FrameStatsPanel
	// -------------------------------------------------------

	void FrameStatsPanel::OnUpdate(const DebugContext& context)
	{
		// Paused frames are dropped rather than buffered, so the graph and the selection hold still
		if (m_Paused)
			return;

		m_RenderFrameTimeHistory.Push(context.Render.Timings.TotalMs);
		m_RenderWaitTimeHistory.Push(context.Render.Timings.PumpWaitMs);

		m_MainFrameTimeHistory.Push(context.Main.TotalMs);
		m_MainWaitTimeHistory.Push(context.Main.PumpWaitMs);

		++m_SampleCount;
	}

	void FrameStatsPanel::OnRender(const DebugContext& context)
	{
		RenderWindow();
	}

	void FrameStatsPanel::RenderWindow()
	{
		ImGui::SetNextWindowSize({ 760.0f, 280.0f }, ImGuiCond_FirstUseEver);
		if (ImGui::Begin(GetName(), &IsOpen))
		{
			const float windowWidth = ImGui::GetContentRegionAvail().x;
			const float detailsWidth = windowWidth * kDetailsWidthFraction;
			const float graphAreaWidth = std::max(windowWidth - detailsWidth - ImGui::GetStyle().ItemSpacing.x, kMinGraphSize);

			// Left: the averages, then the graph filling the rest
			ImGui::BeginChild("##GraphArea", { graphAreaWidth, 0.0f }, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
			{
				// Each thread's toggles beside its average; the colours double as the graph's legend
				auto threadRow = [](const char* name, ImU32 color, float averageMs, bool& show, bool& showWait)
				{
					ImGui::PushID(name);
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::Checkbox("##Show", &show);
					ImGui::SetItemTooltip("Show the %s thread's frame time", name);
					ImGui::SameLine();
					ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(color), "%s Thread Avg Frame Time: %.2f ms", name, averageMs);

					ImGui::TableNextColumn();
					ImGui::Checkbox("Pump wait", &showWait);
					ImGui::SetItemTooltip("Show the %s thread's time blocked in the FramePump, as bars under its line", name);
					ImGui::PopID();
				};

				if (ImGui::BeginTable("##Threads", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX))
				{
					threadRow("Main", kMainColor, m_MainFrameTimeHistory.Average(), m_ShowMain, m_ShowMainWait);
					threadRow("Render", kRenderColor, m_RenderFrameTimeHistory.Average(), m_ShowRender, m_ShowRenderWait);
					ImGui::EndTable();
				}

				// Beside the table, level with its first row
				ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x * 4.0f);
				ImGui::Checkbox("Pause", &m_Paused);

				const ImVec2 available = ImGui::GetContentRegionAvail();
				const ImVec2 graphMin = ImGui::GetCursorScreenPos();
				const ImVec2 graphSize(std::max(available.x, kMinGraphSize), std::max(available.y, kMinGraphSize));

				// Claims the area for hover and clicks; the graph is drawn into it after
				ImGui::InvisibleButton("##UnitGraph", graphSize);

				std::optional<uint32_t> hoveredAge;
				if (ImGui::IsItemHovered())
					hoveredAge = SampleAgeAt(ImGui::GetIO().MousePos.x, graphMin.x, graphSize.x);

				// On press rather than release, so a spike can be caught before it scrolls out from under the cursor
				if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
					m_SelectedFrame = hoveredAge ? std::optional(CaptureFrame(*hoveredAge)) : std::nullopt;

				DrawUnitGraph(ImGui::GetWindowDrawList(), graphMin, graphSize, hoveredAge);
			}
			ImGui::EndChild();

			// Right: the clicked frame
			ImGui::SameLine();
			ImGui::BeginChild("##FrameDetails", { 0.0f, 0.0f }, ImGuiChildFlags_Borders);
			DrawFrameDetails();
			ImGui::EndChild();
		}
		ImGui::End();
	}

	void FrameStatsPanel::DrawUnitGraph(ImDrawList* drawList, const ImVec2& min, const ImVec2& size, std::optional<uint32_t> hoveredAge)
	{
		const ImVec2 max(min.x + size.x, min.y + size.y);
		drawList->AddRectFilled(min, max, IM_COL32(15, 15, 15, 200));
		drawList->PushClipRect(min, max, true);

		// One scale for everything shown. Rises instantly for a spike, eases back down after it scrolls off.
		// A wait never exceeds its own thread's total, but still counts when only the bars are shown.
		float shownMax = kBudgetMs;
		if (m_ShowMain)			shownMax = std::max(shownMax, m_MainFrameTimeHistory.Max());
		if (m_ShowRender)		shownMax = std::max(shownMax, m_RenderFrameTimeHistory.Max());
		if (m_ShowMainWait)		shownMax = std::max(shownMax, m_MainWaitTimeHistory.Max());
		if (m_ShowRenderWait)	shownMax = std::max(shownMax, m_RenderWaitTimeHistory.Max());

		const float targetMax = shownMax * 1.25f;
		m_GraphScaleMax = targetMax > m_GraphScaleMax ? targetMax : std::lerp(m_GraphScaleMax, targetMax, 0.02f);
		auto toY = [&](float ms) { return max.y - (ms / m_GraphScaleMax) * size.y; };

		// Samples are right-aligned: the newest (age 0) sits on the right edge. SampleAgeAt inverts this
		const float step = size.x / static_cast<float>(kMaxHistorySize - 1);
		auto sampleX = [&](uint32_t age) { return min.x + static_cast<float>(kMaxHistorySize - 1 - age) * step; };

		const float budgetY = toY(kBudgetMs);
		drawList->AddLine({ min.x, budgetY }, { max.x, budgetY }, IM_COL32(255, 255, 255, 40));

		// One translucent bar per sample, each a step wide so neighbours meet; drawn first, so lines sit on top.
		// The gap from a bar's top up to its thread's line is that frame's busy time.
		auto drawWaitBars = [&](StatHistory& history, ImU32 color)
		{
			const ImU32 barColor = (color & ~IM_COL32_A_MASK) | IM_COL32(0, 0, 0, 70);
			const float halfStep = step * 0.5f;
			for (uint32_t age = 0; age < history.Count; ++age)
			{
				const float x = sampleX(age);
				drawList->AddRectFilled({ x - halfStep, toY(history.AtAge(age)) }, { x + halfStep, max.y }, barColor);
			}
		};

		if (m_ShowMainWait)
			drawWaitBars(m_MainWaitTimeHistory, kMainColor);
		if (m_ShowRenderWait)
			drawWaitBars(m_RenderWaitTimeHistory, kRenderColor);

		// Faint min/avg/max lines with labels, pushed down when values are close so they don't overlap
		auto drawStatLines = [&](StatHistory& history, ImU32 color, const char* name, bool labelOnRight)
		{
			const struct { const char* Label; float Value; } stats[] = {
				{ "max", history.Max() }, { "avg", history.Average() }, { "min", history.Min() } };	// Top to bottom

			const float lineHeight = ImGui::GetTextLineHeight();
			float nextFreeY = min.y;
			for (const auto& stat : stats)
			{
				const float y = toY(stat.Value);
				drawList->AddLine({ min.x, y }, { max.x, y }, (color & ~IM_COL32_A_MASK) | IM_COL32(0, 0, 0, 90));

				char text[48];
				std::snprintf(text, sizeof(text), "%s %s: %.2f ms", name, stat.Label, stat.Value);
				const float textY = std::max(y - lineHeight, nextFreeY);
				const float textX = labelOnRight ? max.x - ImGui::CalcTextSize(text).x - 4.0f : min.x + 4.0f;
				drawList->AddText({ textX, textY }, color, text);
				nextFreeY = textY + lineHeight;
			}
		};

		// Only the samples written so far, oldest first, so the line scrolls in from the right edge
		auto drawSeries = [&](StatHistory& history, ImU32 color)
		{
			const uint32_t count = history.Count;
			if (count < 2)
				return;

			std::array<ImVec2, kMaxHistorySize> points;
			for (uint32_t i = 0; i < count; ++i)
			{
				const uint32_t age = count - 1 - i;
				points[i] = ImVec2(sampleX(age), toY(history.AtAge(age)));
			}
			drawList->AddPolyline(points.data(), static_cast<int>(count), color, ImDrawFlags_None, 1.5f);
		};

		if (m_ShowMain)
		{
			drawStatLines(m_MainFrameTimeHistory, kMainColor, "Main", false);
			drawSeries(m_MainFrameTimeHistory, kMainColor);
		}
		if (m_ShowRender)
		{
			drawStatLines(m_RenderFrameTimeHistory, kRenderColor, "Render", true);
			drawSeries(m_RenderFrameTimeHistory, kRenderColor);
		}

		// Top centre, clear of the main labels on the left and the render labels on the right
		if (m_Paused)
		{
			constexpr const char* kPausedText = "PAUSED";
			const float textWidth = ImGui::CalcTextSize(kPausedText).x;
			drawList->AddText({ min.x + (size.x - textWidth) * 0.5f, min.y + 4.0f }, IM_COL32(255, 255, 255, 160), kPausedText);
		}

		if (hoveredAge)
		{
			const float x = sampleX(*hoveredAge);
			drawList->AddLine({ x, min.y }, { x, max.y }, kHoverColor);
		}

		// The marker scrolls left with its frame, and stops drawing once the frame falls off the history
		if (m_SelectedFrame)
		{
			const uint64_t age = m_SampleCount - 1 - m_SelectedFrame->SampleNumber;
			if (age < m_MainFrameTimeHistory.Count)
			{
				const float x = sampleX(static_cast<uint32_t>(age));
				drawList->AddLine({ x, min.y }, { x, max.y }, kSelectionColor, 1.5f);
				if (m_ShowMain)
					drawList->AddCircleFilled({ x, toY(m_SelectedFrame->Main.TotalMs) }, 3.5f, kMainColor);
				if (m_ShowRender)
					drawList->AddCircleFilled({ x, toY(m_SelectedFrame->Render.TotalMs) }, 3.5f, kRenderColor);
			}
		}

		drawList->PopClipRect();
	}

	void FrameStatsPanel::DrawFrameDetails()
	{
		if (!m_SelectedFrame)
		{
			ImGui::BeginDisabled();
			ImGui::TextWrapped("Click the graph to inspect a frame");
			ImGui::EndDisabled();
			return;
		}

		const FrameSample& frame = *m_SelectedFrame;
		const uint64_t age = m_SampleCount - 1 - frame.SampleNumber;
		ImGui::Text("Frame %llu", static_cast<unsigned long long>(frame.SampleNumber));
		if (age < m_MainFrameTimeHistory.Count)
			ImGui::TextDisabled("%llu frames ago", static_cast<unsigned long long>(age));
		else
			ImGui::TextDisabled("Scrolled off the graph");

		auto drawThread = [](const char* name, ImU32 color, const ThreadFrameTimings& timings)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, color);
			ImGui::SeparatorText(name);
			ImGui::PopStyleColor();

			if (ImGui::BeginTable(name, 2, ImGuiTableFlags_SizingStretchProp))
			{
				const struct { const char* Label; float Ms; } rows[] = {
					{ "Total", timings.TotalMs }, { "Busy", timings.BusyMs() }, { "Pump wait", timings.PumpWaitMs } };

				for (const auto& row : rows)
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(row.Label);
					ImGui::TableNextColumn();
					ImGui::Text("%.2f ms", row.Ms);
				}
				ImGui::EndTable();
			}
		};

		drawThread("Main", kMainColor, frame.Main);
		drawThread("Render", kRenderColor, frame.Render);
	}

	std::optional<uint32_t> FrameStatsPanel::SampleAgeAt(float x, float graphMinX, float graphWidth) const
	{
		// The inverse of DrawUnitGraph's sampleX, rounded to the nearest sample
		const float step = graphWidth / static_cast<float>(kMaxHistorySize - 1);
		const int slot = std::clamp(static_cast<int>(std::lround((x - graphMinX) / step)), 0, static_cast<int>(kMaxHistorySize) - 1);
		const uint32_t age = static_cast<uint32_t>(kMaxHistorySize - 1 - slot);

		if (age >= m_MainFrameTimeHistory.Count)
			return std::nullopt;	// Left of the oldest sample, while the history is still filling
		return age;
	}

	FrameStatsPanel::FrameSample FrameStatsPanel::CaptureFrame(uint32_t age) const
	{
		// All four histories are pushed together in OnUpdate, so one age indexes the same frame in each
		return {
			.SampleNumber = m_SampleCount - 1 - age,
			.Main = { .TotalMs = m_MainFrameTimeHistory.AtAge(age), .PumpWaitMs = m_MainWaitTimeHistory.AtAge(age) },
			.Render = { .TotalMs = m_RenderFrameTimeHistory.AtAge(age), .PumpWaitMs = m_RenderWaitTimeHistory.AtAge(age) },
		};
	}
}