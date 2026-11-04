// ConsoleManager driven directly, with no debug UI, so every test runs headless.

#include "TestFramework.h"
#include "TestHelpers.h"

#include <Candle/Debug/ConsoleManager.h>

#include <algorithm>
#include <format>
#include <string_view>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Unit;

namespace {

	struct CommandRecord
	{
		int Calls{ 0 };
		std::vector<std::string> Args;
	};

	void RegisterRecording(ConsoleManager& console, std::string name, CommandRecord& record)
	{
		[[maybe_unused]] const ConsoleCommandHandle handle = console.RegisterCommand({
			.Name{ std::move(name) },
			.Callback{ [&record](ConsoleArgs&& args) {
				++record.Calls;
				for (size_t i = 0; i < args.GetCount(); ++i)
					record.Args.emplace_back(args.GetString(i).value_or(""));
			} }
		});
	}

}

CDL_TEST_CASE(ConsoleManager, RunsRegisteredCommand, Unit)
{
	ConsoleManager console;
	CommandRecord record;
	RegisterRecording(console, "echo", record);

	console.ExecuteCommand("echo");

	CDL_EXPECT_EQ(record.Calls, 1);
	CDL_EXPECT(record.Args.empty());
}

CDL_TEST_CASE(ConsoleManager, PassesParsedArgsToCallback, Unit)
{
	ConsoleManager console;
	CommandRecord record;
	RegisterRecording(console, "echo", record);

	console.ExecuteCommand("echo one \"two three\"");

	CDL_CHECK_EQ(record.Args.size(), 2u);
	CDL_EXPECT(record.Args[0] == "one");
	CDL_EXPECT(record.Args[1] == "two three");
}

CDL_TEST_CASE(ConsoleManager, MatchesCommandNamesCaseInsensitively, Unit)
{
	ConsoleManager console;
	CommandRecord record;
	RegisterRecording(console, "Debug", record);

	console.ExecuteCommand("debug");
	console.ExecuteCommand("DEBUG");
	console.ExecuteCommand("Debug");

	CDL_EXPECT_EQ(record.Calls, 3);
}

// An unknown name must not be added by the lookup, or the second attempt would reach the empty callback in slot 0
CDL_TEST_CASE(ConsoleManager, UnknownCommandWarnsEveryTimeAndRunsNothing, Unit)
{
	LoggerFixture logger;
	ConsoleManager console;
	CommandRecord record;
	RegisterRecording(console, "echo", record);

	console.ExecuteCommand("ConsoleManagerTests:missing");
	console.ExecuteCommand("ConsoleManagerTests:missing");
	CDL_CHECK(Logger::Flush());

	const auto warnings = std::ranges::count_if(logger.Records, [](const LogRecord& logRecord) {
		return std::string_view(logRecord.Message) == "Unknown command: ConsoleManagerTests:missing";
		});
	CDL_EXPECT_EQ(warnings, 2);
	CDL_EXPECT_EQ(record.Calls, 0);
}

// More commands than the constructor reserves, so m_Commands reallocates and short names move with it
CDL_TEST_CASE(ConsoleManager, FindsCommandsAfterStorageGrows, Unit)
{
	constexpr int kCommandCount = 64;

	ConsoleManager console;
	std::vector<CommandRecord> records(kCommandCount);
	for (int i = 0; i < kCommandCount; ++i)
		RegisterRecording(console, std::format("cmd{}", i), records[i]);

	for (int i = 0; i < kCommandCount; ++i)
		console.ExecuteCommand(std::format("cmd{}", i));

	for (const CommandRecord& record : records)
		CDL_EXPECT_EQ(record.Calls, 1);
}
