// The async logger. Each capture test owns one Init/Shutdown cycle through LoggerFixture and searches
// for its own marker text, because records pushed while no logger was running wait in the ring and
// surface in whichever fixture drains next.

#include "TestFramework.h"
#include "TestHelpers.h"

#include <charconv>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string_view>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Unit;
using Candle::Test::Type::Stress;

namespace {

	const LogRecord* FindMessage(const std::vector<LogRecord>& records, std::string_view message)
	{
		for (const LogRecord& record : records)
		{
			if (std::string_view(record.Message) == message)
				return &record;
		}
		return nullptr;
	}

	// Parses "LoggerStress <thread> <sequence>".
	bool ParseStressMessage(std::string_view message, int& thread, int& sequence)
	{
		constexpr std::string_view prefix = "LoggerStress ";
		if (!message.starts_with(prefix))
			return false;

		const char* end = message.data() + message.size();
		auto [afterThread, threadError] = std::from_chars(message.data() + prefix.size(), end, thread);
		if (threadError != std::errc() || afterThread == end)
			return false;
		return std::from_chars(afterThread + 1, end, sequence).ec == std::errc();
	}

}

CDL_TEST_CASE(Logger, LevelChannelAndTypeNames, Unit)
{
	CDL_EXPECT_EQ(std::string_view(LogLevelName(LogLevel::Trace)), "TRACE");
	CDL_EXPECT_EQ(std::string_view(LogLevelName(LogLevel::Fatal)), "FATAL");

	CDL_EXPECT_EQ(std::string_view(LogChannelName(LogChannel::None)), "None");
	CDL_EXPECT_EQ(std::string_view(LogChannelName(LogChannel::Application)), "App");
	CDL_EXPECT_EQ(std::string_view(LogChannelName(LogChannel::Math)), "Math");
	// A combined mask is named after its lowest set bit.
	CDL_EXPECT_EQ(std::string_view(LogChannelName(LogChannel::Render | LogChannel::Input)), "Render");

	CDL_EXPECT_EQ(std::string_view(LoggerTypeName(0)), "ENGINE");
	CDL_EXPECT_EQ(std::string_view(LoggerTypeName(1)), "APP");
}

CDL_TEST_CASE(Logger, ChannelOperatorsCombineAndTest, Unit)
{
	const LogChannel combined = LogChannel::Render | LogChannel::Physics;
	CDL_CHECK_NE(combined & LogChannel::Render, 0);
	CDL_CHECK_NE(combined & LogChannel::Physics, 0);
	CDL_CHECK_EQ(combined & LogChannel::Audio, 0);
}

CDL_TEST_CASE(Logger, EnabledFiltersOnLevelAndChannel, Unit)
{
	// Soft checks only, so the defaults are always restored below.
	Logger::SetLevel(LogLevel::Warn);
	Logger::SetChannelMask(static_cast<uint16_t>(LogChannel::Render));

	CDL_EXPECT_FALSE(Logger::Enabled(LogLevel::Info, LogChannel::Render));
	CDL_EXPECT(Logger::Enabled(LogLevel::Warn, LogChannel::Render));
	CDL_EXPECT(Logger::Enabled(LogLevel::Fatal, LogChannel::Render));
	CDL_EXPECT_FALSE(Logger::Enabled(LogLevel::Fatal, LogChannel::Physics));
	CDL_EXPECT_FALSE(Logger::Enabled(LogLevel::Fatal, LogChannel::None));

	Logger::SetLevel(LogLevel::Info);
	Logger::SetChannelMask(0xFFFF);
}

CDL_TEST_CASE(Logger, FlushFailsWhenNotRunning, Unit)
{
	CDL_CHECK_FALSE(Logger::Flush());
}

CDL_TEST_CASE(Logger, RecordCarriesFormattedMessageAndMetadata, Unit)
{
	LoggerFixture logger;
	CDL_CORE_WARN(LogChannel::Render, "LoggerTests: core {} {}", 42, "text");
	CDL_INFO(LogChannel::Physics, "LoggerTests: client marker");
	CDL_CHECK(Logger::Flush());

	const LogRecord* core = FindMessage(logger.Records, "LoggerTests: core 42 text");
	CDL_CHECK(core != nullptr);
	CDL_EXPECT_EQ(core->Level, LogLevel::Warn);
	CDL_EXPECT_EQ(core->Channel, LogChannel::Render);
	CDL_EXPECT_EQ(core->Type, 0);
	CDL_EXPECT_EQ(core->ThreadId, Platform::GetCurrentThreadId());
	CDL_EXPECT_NE(core->Time.Value, 0u);

	const LogRecord* client = FindMessage(logger.Records, "LoggerTests: client marker");
	CDL_CHECK(client != nullptr);
	CDL_EXPECT_EQ(client->Type, 1);
	CDL_EXPECT_EQ(client->Level, LogLevel::Info);
}

CDL_TEST_CASE(Logger, FilteredRecordsNeverReachSinks, Unit)
{
	LoggerFixture logger(LogLevel::Warn, static_cast<uint16_t>(LogChannel::Render));
	CDL_CORE_INFO(LogChannel::Render, "LoggerTests: below level");
	CDL_CORE_ERROR(LogChannel::Physics, "LoggerTests: masked channel");
	CDL_CORE_ERROR(LogChannel::Render, "LoggerTests: kept");
	CDL_CHECK(Logger::Flush());

	CDL_EXPECT(FindMessage(logger.Records, "LoggerTests: kept") != nullptr);
	CDL_EXPECT(FindMessage(logger.Records, "LoggerTests: below level") == nullptr);
	CDL_EXPECT(FindMessage(logger.Records, "LoggerTests: masked channel") == nullptr);
}

CDL_TEST_CASE(Logger, LongMessageIsTruncatedWithEllipsis, Unit)
{
	constexpr size_t capacity = sizeof(LogRecord::Message) - 1;

	LoggerFixture logger;
	const std::string exactFit(capacity, 'a');
	const std::string tooLong(capacity + 60, 'b');
	CDL_CORE_INFO(LogChannel::Application, "{}", exactFit);
	CDL_CORE_INFO(LogChannel::Application, "{}", tooLong);
	CDL_CHECK(Logger::Flush());

	CDL_EXPECT(FindMessage(logger.Records, exactFit) != nullptr);

	const std::string expected = std::string(capacity - 3, 'b') + "...";
	const LogRecord* truncated = FindMessage(logger.Records, expected);
	CDL_CHECK(truncated != nullptr);
	CDL_CHECK_EQ(truncated->Message[capacity], '\0');
}

CDL_TEST_CASE(Logger, FatalIsDeliveredBeforeTheCallReturns, Unit)
{
	// Fatal blocks on the drain thread, because a crash usually follows it. No explicit Flush here.
	LoggerFixture logger;
	CDL_CORE_FATAL(LogChannel::Application, "LoggerTests: fatal marker");

	CDL_CHECK(FindMessage(logger.Records, "LoggerTests: fatal marker") != nullptr);
}

CDL_TEST_CASE(Logger, CanReinitialiseAfterShutdown, Unit)
{
	{
		LoggerFixture first;
		CDL_CORE_INFO(LogChannel::Application, "LoggerTests: first cycle");
		CDL_CHECK(Logger::Flush());
		CDL_CHECK(FindMessage(first.Records, "LoggerTests: first cycle") != nullptr);
	}

	LoggerFixture second;
	CDL_CORE_INFO(LogChannel::Application, "LoggerTests: second cycle");
	CDL_CHECK(Logger::Flush());
	CDL_CHECK(FindMessage(second.Records, "LoggerTests: second cycle") != nullptr);
	CDL_CHECK(FindMessage(second.Records, "LoggerTests: first cycle") == nullptr);
}

CDL_TEST_CASE(Logger, ConcurrentProducersLoseNothingAndKeepPerThreadOrder, Stress)
{
	// Total stays under the ring's 8192 slots so a slow drain wake-up cannot cause legitimate drops.
	const int threadCount = StressThreadCount();
	const int perThread = 8000 / threadCount;

	LoggerFixture logger;
	RunConcurrently(threadCount, [&](int thread) {
		for (int i = 0; i < perThread; ++i)
			CDL_CORE_INFO(LogChannel::Thread, "LoggerStress {} {}", thread, i);
	});
	CDL_CHECK(Logger::Flush());

	std::vector<int> nextExpected(threadCount, 0);
	int received = 0, outOfOrder = 0, dropReports = 0;
	for (const LogRecord& record : logger.Records)
	{
		int thread = 0, sequence = 0;
		if (ParseStressMessage(record.Message, thread, sequence))
		{
			if (sequence != nextExpected[thread])
				++outOfOrder;
			nextExpected[thread] = sequence + 1;
			++received;
		}
		else if (std::string_view(record.Message).starts_with("Logger dropped"))
		{
			++dropReports;
		}
	}

	CDL_EXPECT_EQ(dropReports, 0);
	CDL_EXPECT_EQ(outOfOrder, 0);
	CDL_CHECK_EQ(received, threadCount * perThread);
}

CDL_TEST_CASE(FileSink, WritesFormattedRecordsToFile, Unit)
{
	// A nested path, so the test also covers the sink creating its parent directory.
	const std::filesystem::path directory = std::filesystem::temp_directory_path() / "Candle-Test" / "FileSink";
	const std::filesystem::path path = directory / "nested" / "test.log";
	std::error_code error;
	std::filesystem::remove_all(directory, error);

	{
		FileSink sink(path);
		CDL_CHECK(sink.IsOpen());

		LogRecord record;
		record.Time = Platform::GetTick();
		record.Level = LogLevel::Warn;
		record.Channel = LogChannel::Render;
		record.Type = 1;
		std::snprintf(record.Message, sizeof(record.Message), "FileSinkTests: marker");

		sink.Write(&record, 1);
		sink.Flush();
	}

	std::ifstream file(path, std::ios::binary);
	std::stringstream contents;
	contents << file.rdbuf();
	const std::string text = contents.str();
	file.close();
	std::filesystem::remove_all(directory, error);

	CDL_NOTE(text.substr(0, text.find('\n')));
	CDL_EXPECT(text.contains("FileSinkTests: marker"));
	CDL_EXPECT(text.contains("[APP]"));
	CDL_EXPECT(text.contains("[Render]"));
	CDL_EXPECT(text.contains("[WARN ]"));
	CDL_EXPECT(text.ends_with("\n"));
}
