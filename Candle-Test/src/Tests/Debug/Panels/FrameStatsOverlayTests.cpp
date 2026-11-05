// FrameStatsOverlay's refresh window and command. It draws straight to the foreground list, so what a test can see is
// whether it drew at all, not the numbers.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "PanelTestHelpers.h"

#include <Candle/Debug/Panels/FrameStatsOverlay.h>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Unit;

namespace {

	bool DrewAnything(HeadlessGui& gui, FrameStatsOverlay& overlay)
	{
		bool drew = false;
		gui.Frame([&] {
			overlay.OnRender({});
			drew = ImGui::GetForegroundDrawList()->VtxBuffer.Size > 0;
		});
		return drew;
	}

	DebugContext FrameOf(float seconds)
	{
		DebugContext context;
		context.DeltaTime = seconds;
		return context;
	}

}

CDL_TEST_CASE(FrameStatsOverlay, DrawsNothingBeforeItsFirstWindowCompletes, Unit)
{
	HeadlessGui gui;
	FrameStatsOverlay overlay;

	CDL_EXPECT_FALSE(DrewAnything(gui, overlay));

	overlay.OnUpdate(FrameOf(0.25f));
	CDL_EXPECT_FALSE(DrewAnything(gui, overlay));
}

CDL_TEST_CASE(FrameStatsOverlay, DrawsOnceFramesAddUpToAWindow, Unit)
{
	HeadlessGui gui;
	FrameStatsOverlay overlay;

	overlay.OnUpdate(FrameOf(0.25f));
	overlay.OnUpdate(FrameOf(0.25f));

	CDL_EXPECT(DrewAnything(gui, overlay));
}

CDL_TEST_CASE(FrameStatsOverlay, CommandTogglesWithNoArgs, Unit)
{
	FrameStatsOverlay overlay;
	const DebugPanelCommand command = overlay.GetCommand();

	command.Callback(ConsoleArgs(""));
	CDL_EXPECT(overlay.IsOpen);

	command.Callback(ConsoleArgs(""));
	CDL_EXPECT_FALSE(overlay.IsOpen);
}

CDL_TEST_CASE(FrameStatsOverlay, CommandWithArgsOnlyEverOpens, Unit)
{
	FrameStatsOverlay overlay;
	const DebugPanelCommand command = overlay.GetCommand();

	command.Callback(ConsoleArgs("detailed"));
	command.Callback(ConsoleArgs("detailed"));

	CDL_EXPECT(overlay.IsOpen);
}
