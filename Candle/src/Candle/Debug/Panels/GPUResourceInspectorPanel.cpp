#include "cdlpch.h"
#include "GPUResourceInspectorPanel.h"
#include "Candle/Debug/DebugFormat.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <span>

namespace Candle {

	namespace {

		constexpr float kSampleIntervalSeconds = 1.0f;
		constexpr uint32_t kPendingStuckFrames = 120;	// Packets and frames in flight only hold a release for a handful of frames
		constexpr float kFrameBudgetMs = 1000.0f / 60.0f;
		constexpr float kWarnFraction = 0.8f;
		constexpr float kBadFraction = 0.95f;

		constexpr float kMinSegmentWidth = 2.0f;	// So a handful of slots out of thousands still shows
		constexpr float kMinGraphHeight = 80.0f;	// InvisibleButton asserts on a zero size
		constexpr float kGraphRangePadding = 0.1f;
		constexpr float kGraphMinRangeFraction = 0.01f;	// Of the value, so a flat line doesn't zoom in on float noise
		constexpr float kLegendSpacing = 16.0f;
		constexpr ImVec2 kTableCellPadding(6.0f, 3.0f);
		constexpr ImGuiTableFlags kStatTableFlags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoSavedSettings;

		constexpr ImU32 kCanvasColor = IM_COL32(15, 15, 15, 200);
		constexpr ImU32 kBarBorderColor = IM_COL32(255, 255, 255, 40);
		constexpr ImU32 kAllocatedColor = IM_COL32(70, 140, 230, 255);
		constexpr ImU32 kSlackColor = IM_COL32(70, 140, 230, 90);
		constexpr ImU32 kOutsideVmaColor = IM_COL32(170, 120, 220, 230);
		constexpr ImU32 kLiveColor = IM_COL32(100, 190, 170, 255);
		constexpr ImU32 kPendingColor = IM_COL32(190, 190, 120, 255);
		constexpr ImU32 kMarkerColor = IM_COL32(255, 255, 255, 220);
		constexpr ImU32 kGraphColor = IM_COL32(100, 190, 170, 255);
		constexpr ImU32 kHoverColor = IM_COL32(255, 255, 255, 60);
		constexpr ImU32 kGraphTextColor = IM_COL32(255, 255, 255, 160);
		constexpr ImU32 kWarnColor = IM_COL32(230, 200, 80, 255);	// The same warn and bad colours as FrameStatsOverlay
		constexpr ImU32 kBadColor = IM_COL32(240, 90, 90, 255);

		struct BarSegment
		{
			float Fraction;
			ImU32 Color;
		};

		float Fraction(uint64_t value, uint64_t total)
		{
			return total == 0 ? 0.0f : static_cast<float>(static_cast<double>(value) / static_cast<double>(total));
		}

		uint64_t BytesAbove(uint64_t value, uint64_t floor)
		{
			return value > floor ? value - floor : 0;
		}

		ImU32 TextColor()
		{
			return ImGui::GetColorU32(ImGuiCol_Text);
		}

		ImU32 SeverityColor(float fraction)
		{
			if (fraction >= kBadFraction)
				return kBadColor;
			if (fraction >= kWarnFraction)
				return kWarnColor;
			return TextColor();
		}

		uint64_t TotalAllocationBytes(const GPUAllocatorStats& stats)
		{
			uint64_t total = 0;
			for (uint32_t i = 0; i < stats.HeapCount; ++i)
				total += stats.Heaps[i].AllocationBytes;
			return total;
		}

		// What VMA holds that no pool owns. Saturates since the render thread can free a payload between the two captures.
		uint64_t OtherBytes(const GPUResourceStats& stats)
		{
			return BytesAbove(TotalAllocationBytes(stats.AllocatorStats), stats.GeometryPoolStats.SizeBytes + stats.TexturePoolStats.SizeBytes);
		}

		std::string FormatBytesDelta(uint64_t now, uint64_t then)
		{
			if (now == then)
				return "0 B";
			return now > then ? "+" + FormatBytes(now - then) : "-" + FormatBytes(then - now);
		}

		std::string FormatRate(float perSecond)
		{
			return std::format("{:.1f}/s", perSecond);
		}

		void DrawColoredText(const char* text, ImU32 color)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, color);
			ImGui::TextUnformatted(text);
			ImGui::PopStyleColor();
		}

		void RightAlignInCell(const char* text)
		{
			const float offset = ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(text).x;
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(offset, 0.0f));
		}

		void DrawNumberCell(const std::string& text, ImU32 color)
		{
			RightAlignInCell(text.c_str());
			DrawColoredText(text.c_str(), color);
		}

		void DrawNumberCell(const std::string& text)
		{
			DrawNumberCell(text, TextColor());
		}

		// Sized for its widest value up front: fitted to its content instead, the column would twitch as the digits change
		void SetupNumberColumn(const char* name, const char* widestValue)
		{
			ImGui::TableSetupColumn(name, ImGuiTableColumnFlags_WidthFixed, std::max(ImGui::CalcTextSize(name).x, ImGui::CalcTextSize(widestValue).x));
		}

		// TableHeadersRow, with the number columns' headers lined up with their right-aligned numbers
		void DrawHeadersRow(int firstNumberColumn)
		{
			ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
			for (int column = 0; column < ImGui::TableGetColumnCount(); ++column)
			{
				if (!ImGui::TableSetColumnIndex(column))
					continue;

				const char* name = ImGui::TableGetColumnName(column);
				if (column >= firstNumberColumn)
					RightAlignInCell(name);

				ImGui::PushID(column);
				ImGui::TableHeader(name);
				ImGui::PopID();
			}
		}

		// Segments run left to right as fractions of the bar, and the marker is a tick at its own fraction
		void DrawBar(std::span<const BarSegment> segments, float markerFraction)
		{
			const float width = std::max(ImGui::GetContentRegionAvail().x, 1.0f);
			const float height = ImGui::GetTextLineHeight();
			const ImVec2 min = ImGui::GetCursorScreenPos();
			const ImVec2 max(min.x + width, min.y + height);
			ImGui::Dummy({ width, height });

			ImDrawList* drawList = ImGui::GetWindowDrawList();
			drawList->AddRectFilled(min, max, kCanvasColor);

			float x = min.x;
			for (const BarSegment& segment : segments)
			{
				if (segment.Fraction <= 0.0f)
					continue;

				const float nextX = std::min(x + std::max(segment.Fraction * width, kMinSegmentWidth), max.x);
				drawList->AddRectFilled({ x, min.y }, { nextX, max.y }, segment.Color);
				x = nextX;
			}

			drawList->AddRect(min, max, kBarBorderColor);

			const float markerX = min.x + std::clamp(markerFraction, 0.0f, 1.0f) * width;
			drawList->AddLine({ markerX, min.y }, { markerX, max.y }, kMarkerColor, 2.0f);
		}

		void DrawLegendEntry(ImU32 color, const char* label)
		{
			const float size = ImGui::GetTextLineHeight();
			const ImVec2 min = ImGui::GetCursorScreenPos();
			ImGui::GetWindowDrawList()->AddRectFilled(min, { min.x + size, min.y + size }, color);
			ImGui::Dummy({ size, size });
			ImGui::SameLine();
			ImGui::TextDisabled("%s", label);
			ImGui::SameLine(0.0f, kLegendSpacing);
		}

		void DrawStatTile(const char* label, const std::string& value, ImU32 color, const char* tooltip)
		{
			ImGui::TableNextColumn();
			ImGui::BeginGroup();
			ImGui::TextDisabled("%s", label);
			DrawColoredText(value.c_str(), color);
			ImGui::EndGroup();
			ImGui::SetItemTooltip("%s", tooltip);
		}

	}

	void GPUResourceInspectorPanel::OnUpdate(const DebugContext& context)
	{
		const GPUResourceStats& stats = context.Resources;
		TrackPool(m_MeshActivity, stats.GeometryPoolStats);
		TrackPool(m_TextureActivity, stats.TexturePoolStats);

		// The first capture only starts the window, so what startup uploaded isn't read as this second's rate
		if (!m_SampleStart)
		{
			m_SampleStart = stats;
			m_LastUploadWaitMs = stats.UploaderStats.TotalWaitMs;
			return;
		}

		const double frameWaitMs = stats.UploaderStats.TotalWaitMs - m_LastUploadWaitMs;
		m_LastUploadWaitMs = stats.UploaderStats.TotalWaitMs;
		m_SampleWorstStallMs = std::max(m_SampleWorstStallMs, static_cast<float>(frameWaitMs));

		m_SampleSeconds += context.DeltaTime.GetSeconds();
		if (m_SampleSeconds >= kSampleIntervalSeconds)
			PushSample(stats);
	}

	void GPUResourceInspectorPanel::TrackPool(PoolActivity& activity, const GPUResourcePoolStats& stats)
	{
		activity.FramesSinceRelease = stats.TotalReleases == activity.LastReleases ? activity.FramesSinceRelease + 1 : 0;
		activity.LastReleases = stats.TotalReleases;
	}

	void GPUResourceInspectorPanel::PushSample(const GPUResourceStats& stats)
	{
		const GPUResourceStats& start = *m_SampleStart;
		auto perSecond = [this](uint64_t now, uint64_t then) { return static_cast<float>(now - then) / m_SampleSeconds; };

		m_MeshActivity.ReleasesPerSecond = perSecond(stats.GeometryPoolStats.TotalReleases, start.GeometryPoolStats.TotalReleases);
		m_TextureActivity.ReleasesPerSecond = perSecond(stats.TexturePoolStats.TotalReleases, start.TexturePoolStats.TotalReleases);
		m_UploadsPerSecond = perSecond(stats.UploaderStats.TotalUploads, start.UploaderStats.TotalUploads);
		m_UploadBytesPerSecond = perSecond(stats.UploaderStats.TotalBytes, start.UploaderStats.TotalBytes);
		m_WorstStallMs = m_SampleWorstStallMs;

		ResourceSample& sample = m_History[m_SampleCount % kHistorySize];
		sample[static_cast<size_t>(Metric::VramUsage)] = static_cast<float>(stats.AllocatorStats.CurrentVramUsageBytes);
		sample[static_cast<size_t>(Metric::LiveMeshes)] = static_cast<float>(stats.GeometryPoolStats.LiveSlots);
		sample[static_cast<size_t>(Metric::LiveTextures)] = static_cast<float>(stats.TexturePoolStats.LiveSlots);
		sample[static_cast<size_t>(Metric::PendingSlots)] = static_cast<float>(stats.GeometryPoolStats.PendingSlots + stats.TexturePoolStats.PendingSlots);
		sample[static_cast<size_t>(Metric::UploadRate)] = m_UploadBytesPerSecond;
		sample[static_cast<size_t>(Metric::WorstStall)] = m_WorstStallMs;
		++m_SampleCount;

		m_SampleStart = stats;
		m_SampleSeconds = 0.0f;
		m_SampleWorstStallMs = 0.0f;
	}

	const GPUResourceInspectorPanel::ResourceSample& GPUResourceInspectorPanel::SampleAtAge(uint32_t age) const
	{
		return m_History[(m_SampleCount - 1 - age) % kHistorySize];
	}

	void GPUResourceInspectorPanel::OnRender(const DebugContext& context)
	{
		ImGui::SetNextWindowSize({ 1000.0f, 680.0f }, ImGuiCond_FirstUseEver);
		if (ImGui::Begin(GetName(), &IsOpen))
		{
			const GPUResourceStats& stats = context.Resources;
			RenderToolbar(stats);
			RenderMemorySection(stats.AllocatorStats);
			RenderPoolsSection(stats);
			RenderActivitySection(stats);
			RenderHistorySection();
		}
		ImGui::End();
	}

	void GPUResourceInspectorPanel::RenderToolbar(const GPUResourceStats& stats)
	{
		if (ImGui::Button("Mark baseline"))
		{
			m_Baseline = stats;
			m_BaselineSample = m_SampleCount;
		}
		ImGui::SetItemTooltip("Remember these numbers: do something, undo it, and anything that didn't return to zero under Since baseline leaked");

		if (m_Baseline)
		{
			ImGui::SameLine();
			if (ImGui::Button("Clear"))
				m_Baseline.reset();

			ImGui::SameLine();
			ImGui::AlignTextToFramePadding();
			ImGui::TextDisabled("Marked %llu s ago", static_cast<unsigned long long>(m_SampleCount - m_BaselineSample));
		}
	}

	void GPUResourceInspectorPanel::RenderMemorySection(const GPUAllocatorStats& stats)
	{
		ImGui::SeparatorText("Memory");

		const float budgetUsed = Fraction(stats.CurrentVramUsageBytes, stats.OsVramBudgetBytes);
		const std::string headline = std::format("VRAM {} of {} budget ({:.0f}%)", FormatBytes(stats.CurrentVramUsageBytes), FormatBytes(stats.OsVramBudgetBytes), budgetUsed * 100.0f);
		DrawColoredText(headline.c_str(), SeverityColor(budgetUsed));
		ImGui::SameLine();
		ImGui::TextDisabled("%s on the device", FormatBytes(stats.DeviceMaxVramBytes).c_str());
		if (!stats.HasMemoryBudget)
		{
			ImGui::SameLine();
			ImGui::TextDisabled("(estimated)");
			ImGui::SetItemTooltip("VK_EXT_memory_budget isn't available, so usage is only what VMA holds and the budget is a guess of 80%% of each heap");
		}

		ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, kTableCellPadding);
		if (ImGui::BeginTable("##Heaps", 7, kStatTableFlags))
		{
			ImGui::TableSetupColumn("Heap", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("Usage", ImGuiTableColumnFlags_WidthStretch);
			SetupNumberColumn("Used", "000.00 GB");
			SetupNumberColumn("Budget", "000.00 GB");
			SetupNumberColumn("Size", "000.00 GB");
			SetupNumberColumn("Allocations", "000000");
			SetupNumberColumn("Blocks", "0000");
			DrawHeadersRow(2);

			for (uint32_t i = 0; i < stats.HeapCount; ++i)
				RenderHeapStats(i, stats.Heaps[i]);

			ImGui::EndTable();
		}
		ImGui::PopStyleVar();

		DrawLegendEntry(kAllocatedColor, "Allocated");
		DrawLegendEntry(kSlackColor, "Unused block space");
		DrawLegendEntry(kOutsideVmaColor, "Outside VMA");
		DrawLegendEntry(kMarkerColor, "Budget");
		ImGui::NewLine();
	}

	void GPUResourceInspectorPanel::RenderHeapStats(uint32_t index, const GPUHeapStats& stats)
	{
		const uint64_t slackBytes = BytesAbove(stats.BlockBytes, stats.AllocationBytes);
		const uint64_t outsideVmaBytes = BytesAbove(stats.UsageBytes, stats.BlockBytes);

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("%u: %s", index, stats.HeapType & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT ? "Device local" : "Host");

		ImGui::TableNextColumn();
		const std::array<BarSegment, 3> segments = {
			BarSegment{ Fraction(stats.AllocationBytes, stats.SizeBytes), kAllocatedColor },
			BarSegment{ Fraction(slackBytes, stats.SizeBytes), kSlackColor },
			BarSegment{ Fraction(outsideVmaBytes, stats.SizeBytes), kOutsideVmaColor } };
		DrawBar(segments, Fraction(stats.BudgetBytes, stats.SizeBytes));
		if (ImGui::BeginItemTooltip())
		{
			ImGui::Text("Allocated: %s", FormatBytes(stats.AllocationBytes).c_str());
			ImGui::Text("Unused block space: %s", FormatBytes(slackBytes).c_str());
			ImGui::Text("Outside VMA: %s", FormatBytes(outsideVmaBytes).c_str());
			ImGui::TextDisabled("Outside VMA is the swapchain, pipelines, descriptors and command buffers");
			ImGui::EndTooltip();
		}

		ImGui::TableNextColumn();
		DrawNumberCell(FormatBytes(stats.UsageBytes), SeverityColor(Fraction(stats.UsageBytes, stats.BudgetBytes)));
		ImGui::TableNextColumn();
		DrawNumberCell(FormatBytes(stats.BudgetBytes));
		ImGui::TableNextColumn();
		DrawNumberCell(FormatBytes(stats.SizeBytes));
		ImGui::TableNextColumn();
		DrawNumberCell(std::to_string(stats.AllocationCount));
		ImGui::TableNextColumn();
		DrawNumberCell(std::to_string(stats.BlockCount));
	}

	void GPUResourceInspectorPanel::RenderPoolsSection(const GPUResourceStats& stats)
	{
		ImGui::SeparatorText("Resources");

		ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, kTableCellPadding);
		if (ImGui::BeginTable("##Pools", 10, kStatTableFlags))
		{
			ImGui::TableSetupColumn("Pool", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("Slots", ImGuiTableColumnFlags_WidthStretch);
			SetupNumberColumn("Live", "00000");
			SetupNumberColumn("Pending", "00000");
			SetupNumberColumn("Free", "00000");
			SetupNumberColumn("Peak", "00000");
			SetupNumberColumn("Capacity", "00000");
			SetupNumberColumn("Memory", "000.00 GB");
			SetupNumberColumn("Releases", "0000.0/s");
			SetupNumberColumn("Since baseline", "+00000, +000.0 MB");
			DrawHeadersRow(2);

			RenderPoolStats("Meshes", stats.GeometryPoolStats, m_Baseline ? &m_Baseline->GeometryPoolStats : nullptr, m_MeshActivity);
			RenderPoolStats("Textures", stats.TexturePoolStats, m_Baseline ? &m_Baseline->TexturePoolStats : nullptr, m_TextureActivity);
			RenderOtherMemory(stats);

			ImGui::EndTable();
		}
		ImGui::PopStyleVar();

		DrawLegendEntry(kLiveColor, "Live");
		DrawLegendEntry(kPendingColor, "Pending");
		DrawLegendEntry(kMarkerColor, "Peak");
		ImGui::NewLine();
	}

	void GPUResourceInspectorPanel::RenderPoolStats(const char* poolName, const GPUResourcePoolStats& stats, const GPUResourcePoolStats* baseline, const PoolActivity& activity)
	{
		const float peakUsed = Fraction(stats.MaxUsedSlots, stats.TotalSlots);
		const bool pendingStuck = stats.PendingSlots > 0 && activity.FramesSinceRelease > kPendingStuckFrames;

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(poolName);

		ImGui::TableNextColumn();
		const std::array<BarSegment, 2> segments = {
			BarSegment{ Fraction(stats.LiveSlots, stats.TotalSlots), kLiveColor },
			BarSegment{ Fraction(stats.PendingSlots, stats.TotalSlots), kPendingColor } };
		DrawBar(segments, peakUsed);
		ImGui::SetItemTooltip("%.2f%% of the pool in use, %.2f%% at its peak", Fraction(stats.LiveSlots + stats.PendingSlots, stats.TotalSlots) * 100.0f, peakUsed * 100.0f);

		ImGui::TableNextColumn();
		DrawNumberCell(std::to_string(stats.LiveSlots));

		ImGui::TableNextColumn();
		DrawNumberCell(std::to_string(stats.PendingSlots), pendingStuck ? kWarnColor : TextColor());
		if (pendingStuck)
			ImGui::SetItemTooltip("No release for %u frames, yet these haven't come back. A stalled GPU would stop this panel too, so a slot got lost between the registry and its pool.", activity.FramesSinceRelease);

		ImGui::TableNextColumn();
		DrawNumberCell(std::to_string(stats.FreeSlots));

		ImGui::TableNextColumn();
		DrawNumberCell(std::to_string(stats.MaxUsedSlots), SeverityColor(peakUsed));
		ImGui::SetItemTooltip("The most slots ever live or pending at once. The engine panics if the pool runs out, so raise its capacity before this gets close.");

		ImGui::TableNextColumn();
		DrawNumberCell(std::to_string(stats.TotalSlots));

		ImGui::TableNextColumn();
		DrawNumberCell(FormatBytes(stats.SizeBytes));

		ImGui::TableNextColumn();
		DrawNumberCell(FormatRate(activity.ReleasesPerSecond));

		ImGui::TableNextColumn();
		if (!baseline)
		{
			RightAlignInCell("-");
			ImGui::TextDisabled("-");
			return;
		}

		const int64_t liveDelta = static_cast<int64_t>(stats.LiveSlots) - static_cast<int64_t>(baseline->LiveSlots);
		const bool grew = liveDelta > 0 || stats.SizeBytes > baseline->SizeBytes;
		const std::string liveDeltaText = liveDelta == 0 ? "0" : std::format("{:+d}", liveDelta);
		DrawNumberCell(std::format("{}, {}", liveDeltaText, FormatBytesDelta(stats.SizeBytes, baseline->SizeBytes)), grew ? kWarnColor : TextColor());
	}

	void GPUResourceInspectorPanel::RenderOtherMemory(const GPUResourceStats& stats)
	{
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Other");
		ImGui::SetItemTooltip("Everything VMA holds outside the pools: staging, frame buffers, depth and the GUI. Released resources count here until the GPU retires them.");

		ImGui::TableSetColumnIndex(7);
		const uint64_t otherBytes = OtherBytes(stats);
		DrawNumberCell(FormatBytes(otherBytes));

		ImGui::TableSetColumnIndex(9);
		if (!m_Baseline)
		{
			RightAlignInCell("-");
			ImGui::TextDisabled("-");
			return;
		}

		const uint64_t baselineBytes = OtherBytes(*m_Baseline);
		DrawNumberCell(FormatBytesDelta(otherBytes, baselineBytes), otherBytes > baselineBytes ? kWarnColor : TextColor());
	}

	void GPUResourceInspectorPanel::RenderActivitySection(const GPUResourceStats& stats)
	{
		ImGui::SeparatorText("Activity");

		if (ImGui::BeginTable("##Activity", 5, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
		{
			const GPUUploaderStats& uploads = stats.UploaderStats;
			DrawStatTile("Uploads", FormatRate(m_UploadsPerSecond), TextColor(), "Uploads submitted in the last second");
			DrawStatTile("Upload rate", FormatBytes(static_cast<uint64_t>(m_UploadBytesPerSecond)) + "/s", TextColor(), "Bytes staged for the GPU in the last second");
			DrawStatTile("Worst stall", std::format("{:.2f} ms", m_WorstStallMs), SeverityColor(m_WorstStallMs / kFrameBudgetMs),
				"The longest the main thread spent in one frame of the last second, waiting for an earlier upload to finish");
			DrawStatTile("Uploaded", std::format("{}, {}", uploads.TotalUploads, FormatBytes(uploads.TotalBytes)), TextColor(), "Every upload since startup");
			DrawStatTile("GPU behind by", std::format("{} submits", stats.GraphicsTimelineLag), TextColor(),
				"Graphics submits the GPU hasn't finished, counting uploads as well as frames. If this keeps climbing the GPU isn't keeping up.");
			ImGui::EndTable();
		}
	}

	void GPUResourceInspectorPanel::RenderHistorySection()
	{
		ImGui::SeparatorText("History");

		ImGui::SetNextItemWidth(200.0f);
		if (ImGui::BeginCombo("##Metric", MetricName(m_SelectedMetric)))
		{
			for (size_t i = 0; i < static_cast<size_t>(Metric::Count); ++i)
			{
				const Metric metric = static_cast<Metric>(i);
				if (ImGui::Selectable(MetricName(metric), m_SelectedMetric == metric))
					m_SelectedMetric = metric;
			}
			ImGui::EndCombo();
		}

		const uint32_t sampleCount = static_cast<uint32_t>(std::min<uint64_t>(m_SampleCount, kHistorySize));
		ImGui::SameLine();
		ImGui::TextDisabled("Last %u s", sampleCount);

		DrawHistoryGraph(sampleCount);
	}

	void GPUResourceInspectorPanel::DrawHistoryGraph(uint32_t sampleCount)
	{
		const ImVec2 available = ImGui::GetContentRegionAvail();
		const ImVec2 min = ImGui::GetCursorScreenPos();
		const ImVec2 size(std::max(available.x, kMinGraphHeight), std::max(available.y, kMinGraphHeight));
		const ImVec2 max(min.x + size.x, min.y + size.y);

		ImGui::InvisibleButton("##HistoryGraph", size);
		const bool hovered = ImGui::IsItemHovered();

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		drawList->AddRectFilled(min, max, kCanvasColor);

		if (sampleCount < 2)
		{
			constexpr const char* kWaitingText = "Collecting samples";
			const ImVec2 textSize = ImGui::CalcTextSize(kWaitingText);
			drawList->AddText({ min.x + (size.x - textSize.x) * 0.5f, min.y + (size.y - textSize.y) * 0.5f }, kGraphTextColor, kWaitingText);
			return;
		}

		const size_t metricIndex = static_cast<size_t>(m_SelectedMetric);
		auto valueAtAge = [&](uint32_t age) { return SampleAtAge(age)[metricIndex]; };

		float lowest = valueAtAge(0);
		float highest = lowest;
		for (uint32_t age = 1; age < sampleCount; ++age)
		{
			lowest = std::min(lowest, valueAtAge(age));
			highest = std::max(highest, valueAtAge(age));
		}

		const float padding = std::max({ (highest - lowest) * kGraphRangePadding, highest * kGraphMinRangeFraction, 1.0f });
		const float rangeMin = std::max(lowest - padding, 0.0f);
		const float rangeMax = highest + padding;

		// Stretched over the samples so far, so the first minutes aren't a stub. The newest (age 0) sits on the right edge.
		const float step = size.x / static_cast<float>(sampleCount - 1);
		auto sampleX = [&](uint32_t age) { return max.x - static_cast<float>(age) * step; };
		auto toY = [&](float value) { return max.y - (value - rangeMin) / (rangeMax - rangeMin) * size.y; };

		drawList->PushClipRect(min, max, true);

		std::array<ImVec2, kHistorySize> points;
		for (uint32_t i = 0; i < sampleCount; ++i)
		{
			const uint32_t age = sampleCount - 1 - i;
			points[i] = ImVec2(sampleX(age), toY(valueAtAge(age)));
		}
		drawList->AddPolyline(points.data(), static_cast<int>(sampleCount), kGraphColor, ImDrawFlags_None, 1.5f);

		const float lineHeight = ImGui::GetTextLineHeight();
		drawList->AddText({ min.x + 4.0f, min.y + 2.0f }, kGraphTextColor, FormatMetric(m_SelectedMetric, highest).c_str());
		drawList->AddText({ min.x + 4.0f, max.y - lineHeight - 2.0f }, kGraphTextColor, FormatMetric(m_SelectedMetric, lowest).c_str());

		// The marker scrolls left with the sample it was taken at, and stops drawing once that falls off the history
		if (m_Baseline)
		{
			const uint64_t baselineAge = m_SampleCount - m_BaselineSample;
			if (baselineAge < sampleCount)
			{
				const float x = sampleX(static_cast<uint32_t>(baselineAge));
				drawList->AddLine({ x, min.y }, { x, max.y }, kMarkerColor, 1.5f);
				drawList->AddText({ x + 4.0f, min.y + 2.0f }, kMarkerColor, "Baseline");
			}
		}

		if (hovered)
		{
			const int age = static_cast<int>(std::lround((max.x - ImGui::GetIO().MousePos.x) / step));
			if (age >= 0 && age < static_cast<int>(sampleCount))
			{
				const float x = sampleX(static_cast<uint32_t>(age));
				const float value = valueAtAge(static_cast<uint32_t>(age));
				drawList->AddLine({ x, min.y }, { x, max.y }, kHoverColor);
				drawList->AddCircleFilled({ x, toY(value) }, 3.5f, kGraphColor);
				ImGui::SetTooltip("%s\n%d s ago", FormatMetric(m_SelectedMetric, value).c_str(), age);
			}
		}

		drawList->PopClipRect();
	}

	const char* GPUResourceInspectorPanel::MetricName(Metric metric)
	{
		switch (metric)
		{
		case Metric::VramUsage: return "VRAM usage";
		case Metric::LiveMeshes: return "Live meshes";
		case Metric::LiveTextures: return "Live textures";
		case Metric::PendingSlots: return "Pending slots";
		case Metric::UploadRate: return "Upload rate";
		case Metric::WorstStall: return "Worst upload stall";
		default: break;
		}
		return "Unknown";
	}

	std::string GPUResourceInspectorPanel::FormatMetric(Metric metric, float value)
	{
		switch (metric)
		{
		case Metric::VramUsage: return FormatBytes(static_cast<uint64_t>(value));
		case Metric::UploadRate: return FormatBytes(static_cast<uint64_t>(value)) + "/s";
		case Metric::WorstStall: return std::format("{:.2f} ms", value);
		default: return std::format("{:.0f}", value);
		}
	}

}
