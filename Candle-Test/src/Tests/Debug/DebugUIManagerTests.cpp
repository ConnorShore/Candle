// DebugUIManager on its own console and log sink, with a fake panel to watch. Its constructor needs a real TexturePool for the
// texture inspector, which is the only reason these are Gpu: nothing here renders, and every engine panel stays closed.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "../Render/RenderTestHelpers.h"

#include <Candle/Core/MainThreadLogSink.h>
#include <Candle/Debug/ConsoleManager.h>
#include <Candle/Debug/DebugUIManager.h>

#include <algorithm>
#include <optional>
#include <string_view>
#include <vector>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Gpu;

namespace {

	struct PanelRecord
	{
		int Updates{ 0 };
		int Renders{ 0 };
	};

	class FakePanel : public DebugPanel
	{
	public:
		explicit FakePanel(PanelRecord& record, ConsoleCommandCallback callback = nullptr)
			: m_Record(record), m_Callback(std::move(callback))
		{
		}

		void OnUpdate(const DebugContext& context) override { ++m_Record.Updates; }
		void OnRender(const DebugContext& context) override { ++m_Record.Renders; }

		const char* GetName() const override { return "Fake Panel"; }
		DebugPanelCategory GetCategory() const override { return DebugPanelCategory::General; }
		DebugPanelCommand GetCommand() override { return { .Name{ "Fake" }, .Callback{ m_Callback } }; }

	private:
		PanelRecord& m_Record;
		ConsoleCommandCallback m_Callback;
	};

	// The console outlives the manager, whose panels hold handles into it
	struct TestDebugUI
	{
		TestTextures Textures;
		ConsoleManager Console;
		MainThreadLogSink LogSink{ 64 };
		std::optional<DebugUIManager> Manager{ std::in_place, Console, LogSink, Textures.Pool };
	};

	int CountUnknownCommands(const std::vector<LogRecord>& records)
	{
		return static_cast<int>(std::ranges::count_if(records, [](const LogRecord& record) {
			return std::string_view(record.Message).starts_with("Unknown command");
			}));
	}

}

CDL_TEST_CASE(DebugUIManager, StartsVisibleAndF1Toggles, Gpu)
{
	TestDebugUI test;
	CDL_EXPECT(test.Manager->IsVisible());

	test.Manager->ConsumeEvent(KeyPressedEvent{ KeyCode::F1 });
	CDL_EXPECT_FALSE(test.Manager->IsVisible());

	test.Manager->ConsumeEvent(KeyPressedEvent{ KeyCode::F1 });
	CDL_EXPECT(test.Manager->IsVisible());
}

CDL_TEST_CASE(DebugUIManager, OnlyPressingF1Toggles, Gpu)
{
	TestDebugUI test;

	test.Manager->ConsumeEvent(KeyReleasedEvent{ KeyCode::F1 });
	test.Manager->ConsumeEvent(KeyPressedEvent{ KeyCode::F2 });
	test.Manager->ConsumeEvent(KeyPressedEvent{ KeyCode::Unknown });
	for (const InputEvent& event : kMouseEvents)
		test.Manager->ConsumeEvent(event);

	CDL_EXPECT(test.Manager->IsVisible());
}

CDL_TEST_CASE(DebugUIManager, SetVisibleOverridesTheHotkey, Gpu)
{
	TestDebugUI test;

	test.Manager->SetVisible(false);
	CDL_EXPECT_FALSE(test.Manager->IsVisible());

	test.Manager->SetVisible(true);
	CDL_EXPECT(test.Manager->IsVisible());
}

CDL_TEST_CASE(DebugUIManager, EveryEnginePanelHasACommandUnderDebug, Gpu)
{
	LoggerFixture logger;
	{
		TestDebugUI test;

		for (const std::string_view panel : { "Frame", "FrameGraph", "Console", "TextureInspector", "GPUResources" })
			test.Console.ExecuteCommand(std::format("Debug{}{}", ConsoleManager::kCommandDelimiter, panel));
	}
	CDL_CHECK(Logger::Flush());

	CDL_EXPECT_EQ(CountUnknownCommands(logger.Records), 0);
}

CDL_TEST_CASE(DebugUIManager, UpdatesPanelsWhetherOpenOrNot, Gpu)
{
	TestDebugUI test;
	PanelRecord record;
	test.Manager->AddPanel<FakePanel>(record);

	test.Manager->OnUpdate({});
	test.Console.ExecuteCommand("Debug.Fake");
	test.Manager->OnUpdate({});

	CDL_EXPECT_EQ(record.Updates, 2);
}

CDL_TEST_CASE(DebugUIManager, RendersOnlyOpenPanels, Gpu)
{
	TestDebugUI test;
	PanelRecord record;
	test.Manager->AddPanel<FakePanel>(record);

	test.Manager->OnRender({});
	CDL_EXPECT_EQ(record.Renders, 0);

	test.Console.ExecuteCommand("Debug.Fake");
	test.Manager->OnRender({});
	CDL_EXPECT_EQ(record.Renders, 1);
}

// Hiding is drawing only: panels keep sampling, so their graphs have no gap when the UI comes back
CDL_TEST_CASE(DebugUIManager, HidingStopsRenderingButNotUpdating, Gpu)
{
	TestDebugUI test;
	PanelRecord record;
	test.Manager->AddPanel<FakePanel>(record);
	test.Console.ExecuteCommand("Debug.Fake");

	test.Manager->SetVisible(false);
	test.Manager->OnUpdate({});
	test.Manager->OnRender({});

	CDL_EXPECT_EQ(record.Updates, 1);
	CDL_EXPECT_EQ(record.Renders, 0);
}

CDL_TEST_CASE(DebugUIManager, DefaultCommandTogglesItsPanel, Gpu)
{
	TestDebugUI test;
	PanelRecord record;
	test.Manager->AddPanel<FakePanel>(record);

	test.Console.ExecuteCommand("Debug.Fake");
	test.Manager->OnRender({});
	test.Console.ExecuteCommand("debug.fake");
	test.Manager->OnRender({});

	CDL_EXPECT_EQ(record.Renders, 1);
}

CDL_TEST_CASE(DebugUIManager, PanelsOwnCallbackReplacesTheToggle, Gpu)
{
	TestDebugUI test;
	PanelRecord record;
	int calls = 0;
	test.Manager->AddPanel<FakePanel>(record, [&calls](ConsoleArgs&&) { ++calls; });

	test.Console.ExecuteCommand("Debug.Fake");
	test.Manager->OnRender({});

	CDL_EXPECT_EQ(calls, 1);
	CDL_EXPECT_EQ(record.Renders, 0);
}

CDL_TEST_CASE(DebugUIManager, CommandsGoAwayWithTheManager, Gpu)
{
	LoggerFixture logger;
	{
		TestDebugUI test;
		test.Manager.reset();

		test.Console.ExecuteCommand("Debug.Frame");
	}
	CDL_CHECK(Logger::Flush());

	CDL_EXPECT_EQ(CountUnknownCommands(logger.Records), 1);
}
