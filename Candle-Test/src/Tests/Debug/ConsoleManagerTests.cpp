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

	// Converting to the id would let a temporary handle die at the end of the line and take its command with it
	CDL_STATIC_ASSERT(!std::is_convertible_v<ScopedCommandHandle, ConsoleCommandHandle>, "ScopedCommandHandle must not convert to its id");

	// A copy would be a second owner unregistering the same command
	CDL_STATIC_ASSERT(!std::is_copy_constructible_v<ScopedCommandHandle>);
	CDL_STATIC_ASSERT(!std::is_copy_assignable_v<ScopedCommandHandle>);
	CDL_STATIC_ASSERT(std::is_nothrow_move_constructible_v<ScopedCommandHandle>);
	CDL_STATIC_ASSERT(std::is_nothrow_move_assignable_v<ScopedCommandHandle>);

	struct CommandRecord
	{
		int Calls{ 0 };
		std::vector<std::string> Args;
	};

	// The command only lives as long as the returned handle, so tests have to keep it
	[[nodiscard]] ScopedCommandHandle RegisterRecording(ConsoleManager& console, std::vector<std::string> commandParts, CommandRecord& record)
	{
		return console.RegisterCommand({
			.CommandParts{ std::move(commandParts) },
			.Callback{ [&record](ConsoleArgs&& args) {
				++record.Calls;
				for (size_t i = 0; i < args.GetCount(); ++i)
					record.Args.emplace_back(args.TryGetString(i).value_or(""));
			} }
		});
	}

}

CDL_TEST_CASE(ConsoleManager, RunsRegisteredCommand, Unit)
{
	ConsoleManager console;
	CommandRecord record;
	const ScopedCommandHandle command = RegisterRecording(console, { "echo" }, record);

	console.ExecuteCommand("echo");

	CDL_EXPECT_EQ(record.Calls, 1);
	CDL_EXPECT(record.Args.empty());
}

CDL_TEST_CASE(ConsoleManager, PassesParsedArgsToCallback, Unit)
{
	ConsoleManager console;
	CommandRecord record;
	const ScopedCommandHandle command = RegisterRecording(console, { "echo" }, record);

	console.ExecuteCommand("echo one \"two three\"");

	CDL_CHECK_EQ(record.Args.size(), 2u);
	CDL_EXPECT(record.Args[0] == "one");
	CDL_EXPECT(record.Args[1] == "two three");
}

CDL_TEST_CASE(ConsoleManager, MatchesCommandNamesCaseInsensitively, Unit)
{
	ConsoleManager console;
	CommandRecord record;
	const ScopedCommandHandle command = RegisterRecording(console, { "Debug" }, record);

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
	const ScopedCommandHandle command = RegisterRecording(console, { "echo" }, record);

	console.ExecuteCommand("ConsoleManagerTests:missing");
	console.ExecuteCommand("ConsoleManagerTests:missing");
	CDL_CHECK(Logger::Flush());

	const auto warnings = std::ranges::count_if(logger.Records, [](const LogRecord& logRecord) {
		return std::string_view(logRecord.Message) == "Unknown command: ConsoleManagerTests:missing";
		});
	CDL_EXPECT_EQ(warnings, 2);
	CDL_EXPECT_EQ(record.Calls, 0);
}

CDL_TEST_CASE(ConsoleManager, EmptyLineWarnsAndRunsNothing, Unit)
{
	LoggerFixture logger;
	ConsoleManager console;
	CommandRecord record;
	const ScopedCommandHandle command = RegisterRecording(console, { "echo" }, record);

	console.ExecuteCommand("");
	CDL_CHECK(Logger::Flush());

	const auto warnings = std::ranges::count_if(logger.Records, [](const LogRecord& logRecord) {
		return std::string_view(logRecord.Message) == "No command entered.";
		});
	CDL_EXPECT_EQ(warnings, 1);
	CDL_EXPECT_EQ(record.Calls, 0);
}

CDL_TEST_CASE(ConsoleManager, ExtraSpacesAfterTheCommandAreIgnored, Unit)
{
	ConsoleManager console;
	CommandRecord record;
	const ScopedCommandHandle command = RegisterRecording(console, { "echo" }, record);

	console.ExecuteCommand("echo    one   ");

	CDL_EXPECT_EQ(record.Calls, 1);
	CDL_CHECK_EQ(record.Args.size(), 1u);
	CDL_EXPECT(record.Args[0] == "one");
}

CDL_TEST_CASE(ConsoleManager, TrailingSpaceAddsNoArg, Unit)
{
	ConsoleManager console;
	CommandRecord record;
	const ScopedCommandHandle command = RegisterRecording(console, { "echo" }, record);

	console.ExecuteCommand("echo ");

	CDL_EXPECT_EQ(record.Calls, 1);
	CDL_EXPECT(record.Args.empty());
}

CDL_TEST_CASE(ConsoleManager, TabSeparatesTheCommandFromItsArgs, Unit)
{
	ConsoleManager console;
	CommandRecord record;
	const ScopedCommandHandle command = RegisterRecording(console, { "echo" }, record);

	console.ExecuteCommand("echo\tone");

	CDL_EXPECT_EQ(record.Calls, 1);
	CDL_CHECK_EQ(record.Args.size(), 1u);
	CDL_EXPECT(record.Args[0] == "one");
}

CDL_TEST_CASE(ConsoleManager, NameIsFreeAgainOnceItsCommandIsGone, Unit)
{
	ConsoleManager console;
	CommandRecord firstRecord;
	CommandRecord secondRecord;
	{
		const ScopedCommandHandle first = RegisterRecording(console, { "echo" }, firstRecord);
	}

	const ScopedCommandHandle second = RegisterRecording(console, { "echo" }, secondRecord);
	console.ExecuteCommand("echo");

	CDL_EXPECT_EQ(firstRecord.Calls, 0);
	CDL_EXPECT_EQ(secondRecord.Calls, 1);
}

// The new registration runs first, so the old handle's unregister mustn't take the name away from it
CDL_TEST_CASE(ConsoleManager, ReplacingACommandUnderItsOwnNameKeepsTheNewOne, Unit)
{
	ConsoleManager console;
	CommandRecord oldRecord;
	CommandRecord newRecord;
	ScopedCommandHandle command = RegisterRecording(console, { "echo" }, oldRecord);

	command = RegisterRecording(console, { "echo" }, newRecord);
	console.ExecuteCommand("echo");

	CDL_EXPECT_EQ(oldRecord.Calls, 0);
	CDL_EXPECT_EQ(newRecord.Calls, 1);
}

// Enough commands that m_Commands and the handles both reallocate, which must not unregister anything
CDL_TEST_CASE(ConsoleManager, FindsCommandsAfterStorageGrows, Unit)
{
	constexpr int kCommandCount = 64;

	ConsoleManager console;
	std::vector<CommandRecord> records(kCommandCount);
	std::vector<ScopedCommandHandle> commands;
	for (int i = 0; i < kCommandCount; ++i)
		commands.push_back(RegisterRecording(console, { std::format("cmd{}", i) }, records[i]));

	for (int i = 0; i < kCommandCount; ++i)
		console.ExecuteCommand(std::format("cmd{}", i));

	for (const CommandRecord& record : records)
		CDL_EXPECT_EQ(record.Calls, 1);
}

CDL_TEST_CASE(ConsoleManager, JoinsCommandPartsWithDelimiter, Unit)
{
	ConsoleManager console;
	CommandRecord record;
	const ScopedCommandHandle command = RegisterRecording(console, { "Debug", "Frame" }, record);

	console.ExecuteCommand(std::format("debug{}frame", ConsoleManager::kCommandDelimiter));

	CDL_EXPECT_EQ(record.Calls, 1);
}

CDL_TEST_CASE(ConsoleManager, DestroyingHandleUnregistersCommand, Unit)
{
	ConsoleManager console;
	CommandRecord record;
	{
		const ScopedCommandHandle command = RegisterRecording(console, { "echo" }, record);
		console.ExecuteCommand("echo");
	}

	console.ExecuteCommand("echo");

	CDL_EXPECT_EQ(record.Calls, 1);
}

CDL_TEST_CASE(ConsoleManager, MovedFromHandleLeavesCommandRegistered, Unit)
{
	ConsoleManager console;
	CommandRecord record;
	std::vector<ScopedCommandHandle> commands;
	{
		ScopedCommandHandle original = RegisterRecording(console, { "echo" }, record);
		commands.push_back(std::move(original));
		CDL_EXPECT_EQ(original.GetHandle(), kInvalidCommandHandle);
		CDL_EXPECT_NE(commands[0].GetHandle(), kInvalidCommandHandle);
	}

	console.ExecuteCommand("echo");
	CDL_EXPECT_EQ(record.Calls, 1);

	commands.clear();
	console.ExecuteCommand("echo");
	CDL_EXPECT_EQ(record.Calls, 1);
}

CDL_TEST_CASE(ConsoleManager, MoveAssigningOverHandleUnregistersItsCommand, Unit)
{
	ConsoleManager console;
	CommandRecord replacedRecord;
	CommandRecord keptRecord;
	ScopedCommandHandle command = RegisterRecording(console, { "replaced" }, replacedRecord);

	command = RegisterRecording(console, { "kept" }, keptRecord);

	console.ExecuteCommand("replaced");
	console.ExecuteCommand("kept");

	CDL_EXPECT_EQ(replacedRecord.Calls, 0);
	CDL_EXPECT_EQ(keptRecord.Calls, 1);
}

CDL_TEST_CASE(ConsoleManager, SelfMoveAssignKeepsCommandRegistered, Unit)
{
	ConsoleManager console;
	CommandRecord record;
	ScopedCommandHandle command = RegisterRecording(console, { "echo" }, record);

	ScopedCommandHandle& alias = command;
	command = std::move(alias);

	console.ExecuteCommand("echo");

	CDL_EXPECT_EQ(record.Calls, 1);
}
