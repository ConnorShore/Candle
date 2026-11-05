// FrameGraphPanel's history, seen through the averages it prints above the graph and the frame details beside it.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "PanelTestHelpers.h"

#include <Candle/Debug/Panels/FrameGraphPanel.h>

#include <string>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Unit;

namespace {

	constexpr int kHistorySize = 360;	// FrameGraphPanel::kMaxHistorySize

	DebugContext Sample(float mainMs, float renderMs, float mainWaitMs = 0.0f)
	{
		DebugContext context;
		context.Main = { .TotalMs = mainMs, .PumpWaitMs = mainWaitMs };
		context.Render.Timings.TotalMs = renderMs;
		return context;
	}

	std::string Drawn(HeadlessGui& gui, FrameGraphPanel& panel)
	{
		return gui.Capture([&] { panel.OnRender({}); });
	}

}

CDL_TEST_CASE(FrameGraphPanel, AveragesEachThreadsSamples, Unit)
{
	HeadlessGui gui;
	FrameGraphPanel panel;

	panel.OnUpdate(Sample(10.0f, 4.0f));
	panel.OnUpdate(Sample(20.0f, 8.0f));
	panel.OnUpdate(Sample(30.0f, 12.0f));

	const std::string text = Drawn(gui, panel);
	CDL_EXPECT_MSG(text.contains("Main Thread Avg Frame Time: 20.00 ms"), text);
	CDL_EXPECT_MSG(text.contains("Render Thread Avg Frame Time: 8.00 ms"), text);
}

// One sample is too few for a line, which is a separate path through the graph
CDL_TEST_CASE(FrameGraphPanel, DrawsFromItsFirstSample, Unit)
{
	HeadlessGui gui;
	FrameGraphPanel panel;

	panel.OnUpdate(Sample(5.0f, 5.0f));

	const std::string text = Drawn(gui, panel);
	CDL_EXPECT_MSG(text.contains("Main Thread Avg Frame Time: 5.00 ms"), text);
}

// Half the ring overwritten, so the average only holds together if the wrap replaces the oldest samples
CDL_TEST_CASE(FrameGraphPanel, NewSamplesOverwriteTheOldestOnceTheHistoryIsFull, Unit)
{
	HeadlessGui gui;
	FrameGraphPanel panel;

	for (int i = 0; i < kHistorySize; ++i)
		panel.OnUpdate(Sample(100.0f, 100.0f));
	for (int i = 0; i < kHistorySize / 2; ++i)
		panel.OnUpdate(Sample(10.0f, 10.0f));

	const std::string text = Drawn(gui, panel);
	CDL_EXPECT_MSG(text.contains("Main Thread Avg Frame Time: 55.00 ms"), text);
}

CDL_TEST_CASE(FrameGraphPanel, ClickedFrameShowsItsTimings, Unit)
{
	HeadlessGui gui;
	FrameGraphPanel panel;
	for (int i = 0; i < kHistorySize; ++i)
		panel.OnUpdate(Sample(20.0f, 8.0f, 5.0f));

	auto draw = [&] {
		ImGui::SetNextWindowPos({ 0.0f, 0.0f });
		panel.OnRender({});
	};

	CDL_EXPECT(gui.Capture(draw).contains("Click the graph to inspect a frame"));

	gui.Click({ 300.0f, 200.0f }, draw);	// Well inside the graph, whichever sample it lands on

	const std::string text = gui.Capture(draw);
	CDL_EXPECT_MSG(text.contains("frames ago"), text);
	CDL_EXPECT_MSG(text.contains("20.00 ms"), text);
	CDL_EXPECT_MSG(text.contains("15.00 ms"), text);	// Busy: the total less the pump wait
	CDL_EXPECT_MSG(text.contains("5.00 ms"), text);
}

// The details are a copy, so they outlive the sample once the graph has scrolled past it
CDL_TEST_CASE(FrameGraphPanel, SelectionSurvivesScrollingOffTheGraph, Unit)
{
	HeadlessGui gui;
	FrameGraphPanel panel;
	for (int i = 0; i < kHistorySize; ++i)
		panel.OnUpdate(Sample(20.0f, 8.0f));

	auto draw = [&] {
		ImGui::SetNextWindowPos({ 0.0f, 0.0f });
		panel.OnRender({});
	};
	gui.Click({ 300.0f, 200.0f }, draw);

	for (int i = 0; i < kHistorySize; ++i)
		panel.OnUpdate(Sample(1.0f, 1.0f));

	const std::string text = gui.Capture(draw);
	CDL_EXPECT_MSG(text.contains("Scrolled off the graph"), text);
	CDL_EXPECT_MSG(text.contains("20.00 ms"), text);
}
