// DebugConsolePanel end to end: records through a real sink, what it draws of them, and lines typed into its real input.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "PanelTestHelpers.h"

#include <Candle/Core/MainThreadLogSink.h>
#include <Candle/Debug/ConsoleManager.h>
#include <Candle/Debug/Panels/DebugConsolePanel.h>

#include <format>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Unit;

namespace {

	constexpr int kMaxEntries = 4096;	// DebugConsolePanel::kMaxEntries

	LogRecord MakeRecord(std::string_view message, LogChannel channel = LogChannel::Application)
	{
		LogRecord record;
		record.Time = Platform::GetTick();
		record.Channel = channel;
		message.copy(record.Message, sizeof(record.Message) - 1);
		return record;
	}

	struct TestConsole
	{
		HeadlessGui Gui;
		ConsoleManager Console;
		MainThreadLogSink Sink{ 8192 };
		DebugConsolePanel Panel{ Console, Sink };

		void Log(const LogRecord& record)
		{
			Sink.Write(&record, 1);
			Sink.Dispatch();
		}

		void Draw() { Gui.Frame([this] { Panel.OnRender({}); }); }
		std::string Drawn() { return Gui.Capture([this] { Panel.OnRender({}); }); }

		// The way a user opens it: the panel focuses its input only when the hotkey opened it
		void OpenWithHotkey()
		{
			DebugContext context;
			context.Input.KeysPressed.set(std::to_underlying(KeyCode::GraveAccent));
			Panel.OnUpdate(context);

			Draw();
			Draw();
		}

		void TypeLine(const char* line)
		{
			ImGuiIO& io = ImGui::GetIO();
			io.AddInputCharactersUTF8(line);
			Draw();
			io.AddKeyEvent(ImGuiKey_Enter, true);
			Draw();
			io.AddKeyEvent(ImGuiKey_Enter, false);
			Draw();
		}
	};

	size_t CountOf(std::string_view text, std::string_view pattern)
	{
		size_t count = 0;
		for (size_t at = text.find(pattern); at != std::string_view::npos; at = text.find(pattern, at + pattern.size()))
			++count;
		return count;
	}

	[[nodiscard]] ScopedCommandHandle RegisterRecording(ConsoleManager& console, std::vector<std::string>& received)
	{
		return console.RegisterCommand({
			.CommandParts{ "echo" },
			.Callback{ [&received](ConsoleArgs&& args) { received.emplace_back(args.TryGetString(0).value_or("")); } }
		});
	}

}

CDL_TEST_CASE(DebugConsolePanel, HotkeyTogglesTheConsole, Unit)
{
	TestConsole test;
	DebugContext hotkey;
	hotkey.Input.KeysPressed.set(std::to_underlying(KeyCode::GraveAccent));

	test.Panel.OnUpdate(hotkey);
	CDL_EXPECT(test.Panel.IsOpen);

	test.Panel.OnUpdate(hotkey);
	CDL_EXPECT_FALSE(test.Panel.IsOpen);
}

// Only the edge counts, so holding the key doesn't flicker the console every frame
CDL_TEST_CASE(DebugConsolePanel, OtherInputLeavesItAlone, Unit)
{
	TestConsole test;
	DebugContext context;
	context.Input.KeysPressed.set(std::to_underlying(KeyCode::A));
	context.Input.KeysDown.set(std::to_underlying(KeyCode::GraveAccent));

	test.Panel.OnUpdate(context);

	CDL_EXPECT_FALSE(test.Panel.IsOpen);
}

CDL_TEST_CASE(DebugConsolePanel, ShowsARecordsChannelLevelAndMessage, Unit)
{
	TestConsole test;

	LogRecord record = MakeRecord("console marker", LogChannel::Render);
	record.Level = LogLevel::Warn;
	test.Log(record);

	const std::string text = test.Drawn();
	CDL_EXPECT_MSG(text.contains("console marker"), text);
	CDL_EXPECT_MSG(text.contains("[Render]"), text);
	CDL_EXPECT_MSG(text.contains("[WARN ]"), text);
}

// The bit picks the colour slot, so the last channel is the one that would run off the end
CDL_TEST_CASE(DebugConsolePanel, DrawsEveryChannel, Unit)
{
	TestConsole test;
	for (size_t i = 0; i < kLogChannelCount; ++i)
		test.Log(MakeRecord("line", static_cast<LogChannel>(1u << i)));
	test.Log(MakeRecord("line", LogChannel::None));

	const std::string text = test.Drawn();
	for (size_t i = 0; i < kLogChannelCount; ++i)
	{
		const std::string name = std::format("[{}]", LogChannelName(static_cast<LogChannel>(1u << i)));
		CDL_EXPECT_MSG(text.contains(name), name);
	}
	CDL_EXPECT_MSG(text.contains("[None]"), text);
}

// A dispatch can end between a message's parts, so the first part waits for the rest instead of showing cut off
CDL_TEST_CASE(DebugConsolePanel, JoinsAMessagesPartsAcrossDispatches, Unit)
{
	TestConsole test;

	LogRecord first = MakeRecord("first part, ");
	first.HasNext = true;
	test.Log(first);
	CDL_EXPECT_FALSE(test.Drawn().contains("first part"));

	LogRecord second = MakeRecord("second part");
	second.IsContinuation = true;
	test.Log(second);

	const std::string text = test.Drawn();
	CDL_EXPECT_MSG(text.contains("first part, second part"), text);
}

CDL_TEST_CASE(DebugConsolePanel, MultiLineMessageShowsItsHeaderOnce, Unit)
{
	TestConsole test;

	test.Log(MakeRecord("line one\nline two\nline three\n"));

	const std::string text = test.Drawn();
	CDL_EXPECT_MSG(text.contains("line one"), text);
	CDL_EXPECT_MSG(text.contains("line two"), text);
	CDL_EXPECT_MSG(text.contains("line three"), text);
	CDL_EXPECT_EQ(CountOf(text, "[INFO ]"), 1u);
}

CDL_TEST_CASE(DebugConsolePanel, KeepsOnlyTheNewestEntries, Unit)
{
	constexpr int kOverflow = 5;

	TestConsole test;
	for (int i = 0; i < kMaxEntries + kOverflow; ++i)
	{
		const LogRecord record = MakeRecord(std::format("entry {};", i));
		test.Sink.Write(&record, 1);
	}
	test.Sink.Dispatch();

	const std::string text = test.Drawn();
	CDL_EXPECT_EQ(CountOf(text, "[INFO ]"), static_cast<size_t>(kMaxEntries));
	CDL_EXPECT_FALSE(text.contains(std::format("entry {};", kOverflow - 1)));
	CDL_EXPECT(text.contains(std::format("entry {};", kOverflow)));
	CDL_EXPECT(text.contains(std::format("entry {};", kMaxEntries + kOverflow - 1)));
}

// A listener left behind would write into the destroyed panel. Registered first, it would also get the record before this one.
CDL_TEST_CASE(DebugConsolePanel, StopsListeningOnceDestroyed, Unit)
{
	ConsoleManager console;
	MainThreadLogSink sink(16);
	{
		DebugConsolePanel panel(console, sink);
	}

	int delivered = 0;
	sink.RegisterListener([&](std::span<const LogRecord> records) { delivered += static_cast<int>(records.size()); });

	const LogRecord record = MakeRecord("after");
	sink.Write(&record, 1);
	sink.Dispatch();

	CDL_EXPECT_EQ(delivered, 1);
}

CDL_TEST_CASE(DebugConsolePanel, TypedLineRunsTheCommand, Unit)
{
	TestConsole test;
	std::vector<std::string> received;
	const ScopedCommandHandle command = RegisterRecording(test.Console, received);

	test.OpenWithHotkey();
	test.TypeLine("echo hello");

	CDL_CHECK_EQ(received.size(), 1u);
	CDL_EXPECT_EQ(received[0], "hello");
}

// ConsoleManager reads a leading space as an empty command name, so the panel has to trim before it gets there
CDL_TEST_CASE(DebugConsolePanel, TypedLineIsTrimmed, Unit)
{
	TestConsole test;
	std::vector<std::string> received;
	const ScopedCommandHandle command = RegisterRecording(test.Console, received);

	test.OpenWithHotkey();
	test.TypeLine("   echo trimmed  ");

	CDL_CHECK_EQ(received.size(), 1u);
	CDL_EXPECT_EQ(received[0], "trimmed");
}

CDL_TEST_CASE(DebugConsolePanel, BlankLineRunsNothing, Unit)
{
	LoggerFixture logger;
	CDL_CHECK(Logger::Flush());
	const size_t earlierRecords = logger.Records.size();	// Whatever earlier tests left in the ring

	{
		TestConsole test;
		test.OpenWithHotkey();
		test.TypeLine("    ");
	}
	CDL_CHECK(Logger::Flush());

	for (size_t i = earlierRecords; i < logger.Records.size(); ++i)
		CDL_EXPECT_MSG(logger.Records[i].Channel != LogChannel::Command, logger.Records[i].Message);
}

// Input is cleared and refocused on submit, so the next command can be typed straight away and Enter alone repeats nothing
CDL_TEST_CASE(DebugConsolePanel, SubmittingClearsTheInputAndKeepsItFocused, Unit)
{
	TestConsole test;
	std::vector<std::string> received;
	const ScopedCommandHandle command = RegisterRecording(test.Console, received);

	test.OpenWithHotkey();
	test.TypeLine("echo first");
	test.TypeLine("");
	test.TypeLine("echo second");

	CDL_CHECK_EQ(received.size(), 2u);
	CDL_EXPECT_EQ(received[1], "second");
}
