// GPUResourceInspectorPanel fed made-up stats, which is why this needs no GPU. It samples once a second, so a test advances
// DeltaTime by hand and reads the rates back from what the panel draws.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "PanelTestHelpers.h"

#include <Candle/Debug/Panels/GPUResourceInspectorPanel.h>

#include <string>
#include <string_view>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Unit;

namespace {

	constexpr uint64_t kMegabyte = 1024 * 1024;
	constexpr uint64_t kGigabyte = 1024 * kMegabyte;
	constexpr int kHistorySize = 300;	// GPUResourceInspectorPanel::kHistorySize

	void Update(GPUResourceInspectorPanel& panel, const GPUResourceStats& stats, float seconds = 1.0f)
	{
		DebugContext context;
		context.DeltaTime = seconds;
		context.Resources = stats;
		panel.OnUpdate(context);
	}

	std::string Drawn(HeadlessGui& gui, GPUResourceInspectorPanel& panel, const GPUResourceStats& stats = {})
	{
		DebugContext context;
		context.Resources = stats;
		return gui.Capture([&] { panel.OnRender(context); });
	}

	// The captured line that starts with name, so a number can't be matched from some other row
	std::string RowOf(const std::string& text, std::string_view name)
	{
		size_t start = text.find(name);
		if (start == std::string::npos)
			return {};
		return text.substr(start, text.find('\n', start) - start);
	}

}

CDL_TEST_CASE(GPUResourceInspectorPanel, ShowsVramAgainstItsBudget, Unit)
{
	HeadlessGui gui;
	GPUResourceInspectorPanel panel;

	GPUResourceStats stats;
	stats.AllocatorStats.CurrentVramUsageBytes = kGigabyte;
	stats.AllocatorStats.OsVramBudgetBytes = 4 * kGigabyte;

	const std::string text = Drawn(gui, panel, stats);
	CDL_EXPECT_MSG(text.contains("VRAM 1.00 GB of 4.00 GB budget (25%)"), text);
}

CDL_TEST_CASE(GPUResourceInspectorPanel, EmptyStatsDoNotDivideByZero, Unit)
{
	HeadlessGui gui;
	GPUResourceInspectorPanel panel;

	const std::string text = Drawn(gui, panel);
	CDL_EXPECT_MSG(text.contains("VRAM 0 B of 0 B budget (0%)"), text);
}

CDL_TEST_CASE(GPUResourceInspectorPanel, ShowsEachHeapsNumbers, Unit)
{
	HeadlessGui gui;
	GPUResourceInspectorPanel panel;

	GPUResourceStats stats;
	stats.AllocatorStats.HeapCount = 2;
	stats.AllocatorStats.Heaps[0] = { .HeapType = VK_MEMORY_HEAP_DEVICE_LOCAL_BIT, .BlockCount = 3, .AllocationCount = 12,
		.SizeBytes = 8 * kGigabyte, .UsageBytes = 2 * kGigabyte, .BudgetBytes = 6 * kGigabyte };
	stats.AllocatorStats.Heaps[1] = { .SizeBytes = 16 * kGigabyte };

	const std::string text = Drawn(gui, panel, stats);
	const std::string deviceHeap = RowOf(text, "0: Device local");
	CDL_EXPECT_MSG(deviceHeap.contains("2.00 GB"), text);
	CDL_EXPECT_MSG(deviceHeap.contains("6.00 GB"), text);
	CDL_EXPECT_MSG(deviceHeap.contains("8.00 GB"), text);
	CDL_EXPECT_MSG(deviceHeap.contains("12"), text);
	CDL_EXPECT_MSG(RowOf(text, "1: Host").contains("16.00 GB"), text);
}

CDL_TEST_CASE(GPUResourceInspectorPanel, ShowsEachPoolsSlots, Unit)
{
	HeadlessGui gui;
	GPUResourceInspectorPanel panel;

	GPUResourceStats stats;
	stats.GeometryPoolStats = { .SizeBytes = 3 * kMegabyte, .TotalSlots = 16000, .LiveSlots = 501, .PendingSlots = 22, .FreeSlots = 15477, .MaxUsedSlots = 640 };
	stats.TexturePoolStats = { .TotalSlots = 8000, .LiveSlots = 77 };

	const std::string text = Drawn(gui, panel, stats);
	const std::string meshes = RowOf(text, "Meshes");
	for (const std::string_view value : { "501", "22", "15477", "640", "16000", "3.0 MB" })
		CDL_EXPECT_MSG(meshes.contains(value), text);
	CDL_EXPECT_MSG(RowOf(text, "Textures").contains("8000"), text);
}

// Startup's uploads happened before the panel existed, so the first capture only opens the window
CDL_TEST_CASE(GPUResourceInspectorPanel, FirstCaptureOnlyStartsTheRateWindow, Unit)
{
	HeadlessGui gui;
	GPUResourceInspectorPanel panel;

	GPUResourceStats stats;
	stats.UploaderStats.TotalUploads = 100;
	stats.UploaderStats.TotalBytes = 100 * kMegabyte;
	Update(panel, stats);

	stats.UploaderStats.TotalUploads = 105;
	stats.UploaderStats.TotalBytes = 102 * kMegabyte;
	Update(panel, stats);

	const std::string text = Drawn(gui, panel, stats);
	CDL_EXPECT_MSG(text.contains("5.0/s"), text);
	CDL_EXPECT_MSG(text.contains("2.0 MB/s"), text);
	CDL_EXPECT_MSG(text.contains("105, 102.0 MB"), text);
}

CDL_TEST_CASE(GPUResourceInspectorPanel, RatesCoverTheWholeSampleNotItsLastFrame, Unit)
{
	HeadlessGui gui;
	GPUResourceInspectorPanel panel;
	GPUResourceStats stats;
	Update(panel, stats);

	for (int frame = 0; frame < 4; ++frame)
	{
		stats.UploaderStats.TotalUploads += 2;
		Update(panel, stats, 0.25f);
	}

	const std::string text = Drawn(gui, panel, stats);
	CDL_EXPECT_MSG(text.contains("8.0/s"), text);
}

CDL_TEST_CASE(GPUResourceInspectorPanel, ReleaseRatesAreTrackedPerPool, Unit)
{
	HeadlessGui gui;
	GPUResourceInspectorPanel panel;
	GPUResourceStats stats;
	Update(panel, stats);

	stats.GeometryPoolStats.TotalReleases = 3;
	stats.TexturePoolStats.TotalReleases = 7;
	Update(panel, stats);

	const std::string text = Drawn(gui, panel, stats);
	CDL_EXPECT_MSG(RowOf(text, "Meshes").contains("3.0/s"), text);
	CDL_EXPECT_MSG(RowOf(text, "Textures").contains("7.0/s"), text);
}

// An average would bury the one frame that hitched, so this is the worst single frame's share of the running total
CDL_TEST_CASE(GPUResourceInspectorPanel, WorstStallIsTheLongestSingleFrame, Unit)
{
	HeadlessGui gui;
	GPUResourceInspectorPanel panel;
	GPUResourceStats stats;
	stats.UploaderStats.TotalWaitMs = 1000.0;	// Startup's waiting, which no frame of the sample did
	Update(panel, stats);

	for (const double waitMs : { 1.0, 5.0, 2.0, 0.0 })
	{
		stats.UploaderStats.TotalWaitMs += waitMs;
		Update(panel, stats, 0.25f);
	}

	const std::string text = Drawn(gui, panel, stats);
	CDL_EXPECT_MSG(text.contains("5.00 ms"), text);
}

CDL_TEST_CASE(GPUResourceInspectorPanel, WorstStallResetsEachSample, Unit)
{
	HeadlessGui gui;
	GPUResourceInspectorPanel panel;
	GPUResourceStats stats;
	Update(panel, stats);

	stats.UploaderStats.TotalWaitMs += 9.0;
	Update(panel, stats);
	stats.UploaderStats.TotalWaitMs += 4.0;
	Update(panel, stats);

	const std::string text = Drawn(gui, panel, stats);
	CDL_EXPECT_MSG(text.contains("4.00 ms"), text);
	CDL_EXPECT_FALSE(text.contains("9.00 ms"));
}

CDL_TEST_CASE(GPUResourceInspectorPanel, ShowsHowFarBehindTheGpuIs, Unit)
{
	HeadlessGui gui;
	GPUResourceInspectorPanel panel;

	GPUResourceStats stats;
	stats.GraphicsTimelineLag = 4;

	const std::string text = Drawn(gui, panel, stats);
	CDL_EXPECT_MSG(text.contains("4 submits"), text);
}

CDL_TEST_CASE(GPUResourceInspectorPanel, HistoryCountsSamplesUpToItsSize, Unit)
{
	HeadlessGui gui;
	GPUResourceInspectorPanel panel;
	GPUResourceStats stats;
	Update(panel, stats);

	for (int i = 0; i < 3; ++i)
		Update(panel, stats);
	CDL_EXPECT(Drawn(gui, panel, stats).contains("Last 3 s"));

	for (int i = 0; i < kHistorySize + 50; ++i)
		Update(panel, stats);

	const std::string text = Drawn(gui, panel, stats);
	CDL_EXPECT_MSG(text.contains("Last 300 s"), text);
}

// What VMA holds minus what the pools own. The render thread can free a payload between the two captures, which would wrap.
CDL_TEST_CASE(GPUResourceInspectorPanel, OtherMemoryIsWhatNoPoolOwnsAndNeverWraps, Unit)
{
	HeadlessGui gui;
	GPUResourceInspectorPanel panel;

	GPUResourceStats stats;
	stats.AllocatorStats.HeapCount = 1;
	stats.AllocatorStats.Heaps[0].AllocationBytes = 5 * kMegabyte;
	stats.GeometryPoolStats.SizeBytes = 2 * kMegabyte;
	stats.TexturePoolStats.SizeBytes = 1 * kMegabyte;
	CDL_EXPECT_MSG(RowOf(Drawn(gui, panel, stats), "Other").contains("2.0 MB"), "");

	stats.AllocatorStats.Heaps[0].AllocationBytes = 1 * kMegabyte;
	const std::string other = RowOf(Drawn(gui, panel, stats), "Other");
	CDL_EXPECT_MSG(other.contains("0 B") && !other.contains("GB"), other);
}

CDL_TEST_CASE(GPUResourceInspectorPanel, BaselineShowsGrowthSinceItWasMarked, Unit)
{
	HeadlessGui gui;
	GPUResourceInspectorPanel panel;

	GPUResourceStats stats;
	stats.GeometryPoolStats = { .SizeBytes = 2 * kMegabyte, .LiveSlots = 10 };
	stats.TexturePoolStats = { .SizeBytes = 4 * kMegabyte, .LiveSlots = 20 };

	CDL_EXPECT_FALSE(RowOf(Drawn(gui, panel, stats), "Meshes").contains("+"));

	// "Mark baseline" is the first thing in the window
	auto draw = [&] {
		DebugContext context;
		context.Resources = stats;
		ImGui::SetNextWindowPos({ 0.0f, 0.0f });
		panel.OnRender(context);
	};
	const ImGuiStyle& style = ImGui::GetStyle();
	gui.Click({ style.WindowPadding.x + 8.0f, ImGui::GetFrameHeight() + style.WindowPadding.y + 8.0f }, draw);

	stats.GeometryPoolStats = { .SizeBytes = 3 * kMegabyte, .LiveSlots = 13 };
	stats.TexturePoolStats = { .SizeBytes = 3 * kMegabyte, .LiveSlots = 18 };

	const std::string text = gui.Capture(draw);
	CDL_EXPECT_MSG(RowOf(text, "Meshes").contains("+3, +1.0 MB"), text);
	CDL_EXPECT_MSG(RowOf(text, "Textures").contains("-2, -1.0 MB"), text);
	CDL_EXPECT_MSG(text.contains("Marked 0 s ago"), text);
}
