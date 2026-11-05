// MainThreadLogSink driven directly, then fed by its one real producer, the logger's drain thread.

#include "TestFramework.h"
#include "TestHelpers.h"

#include <Candle/Core/MainThreadLogSink.h>

#include <algorithm>
#include <charconv>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Unit;
using Candle::Test::Type::Stress;

namespace {

	constexpr size_t kCapacity = 256;

	void Write(MainThreadLogSink& sink, std::string_view message)
	{
		LogRecord record;
		message.copy(record.Message, sizeof(record.Message) - 1);
		sink.Write(&record, 1);
	}

	void WriteNumbered(MainThreadLogSink& sink, int count)
	{
		for (int i = 0; i < count; ++i)
			Write(sink, std::to_string(i));
	}

	std::vector<std::string> Numbered(int count)
	{
		std::vector<std::string> messages;
		for (int i = 0; i < count; ++i)
			messages.push_back(std::to_string(i));
		return messages;
	}

	// Copies out of each batch, since the span only lives for the call
	struct Received
	{
		int Calls{ 0 };
		std::vector<std::string> Messages;

		LogListener Listener()
		{
			return [this](std::span<const LogRecord> records) {
				++Calls;
				for (const LogRecord& record : records)
					Messages.emplace_back(record.Message);
			};
		}
	};

}

CDL_TEST_CASE(MainThreadLogSink, DispatchWithNothingPendingCallsNoListener, Unit)
{
	MainThreadLogSink sink(kCapacity);
	Received received;
	sink.RegisterListener(received.Listener());

	sink.Dispatch();

	CDL_EXPECT_EQ(received.Calls, 0);
}

CDL_TEST_CASE(MainThreadLogSink, DeliversRecordsInTheOrderTheyWereWritten, Unit)
{
	MainThreadLogSink sink(kCapacity);
	Received received;
	sink.RegisterListener(received.Listener());

	WriteNumbered(sink, 8);
	sink.Dispatch();

	CDL_EXPECT(received.Messages == Numbered(8));
}

CDL_TEST_CASE(MainThreadLogSink, DeliversEachRecordOnce, Unit)
{
	MainThreadLogSink sink(kCapacity);
	Received received;
	sink.RegisterListener(received.Listener());

	WriteNumbered(sink, 3);
	sink.Dispatch();
	sink.Dispatch();

	CDL_EXPECT_EQ(received.Messages.size(), 3u);
}

// Larger than the batch Dispatch pops at once, so one call has to loop until the sink is empty
CDL_TEST_CASE(MainThreadLogSink, OneDispatchDrainsAWholeBacklog, Unit)
{
	constexpr int kBacklog = 200;

	MainThreadLogSink sink(kCapacity);
	Received received;
	sink.RegisterListener(received.Listener());

	WriteNumbered(sink, kBacklog);
	sink.Dispatch();

	CDL_EXPECT(received.Messages == Numbered(kBacklog));
}

// The sink starts with the logger and the console panel only later, so startup's records have to wait for it
CDL_TEST_CASE(MainThreadLogSink, RecordsWaitForTheFirstListener, Unit)
{
	MainThreadLogSink sink(kCapacity);
	WriteNumbered(sink, 3);
	sink.Dispatch();

	Received received;
	sink.RegisterListener(received.Listener());
	sink.Dispatch();

	CDL_EXPECT(received.Messages == Numbered(3));
}

CDL_TEST_CASE(MainThreadLogSink, EveryListenerReceivesEveryRecord, Unit)
{
	MainThreadLogSink sink(kCapacity);
	Received first, second;
	sink.RegisterListener(first.Listener());
	sink.RegisterListener(second.Listener());

	WriteNumbered(sink, 100);
	sink.Dispatch();

	CDL_EXPECT(first.Messages == Numbered(100));
	CDL_EXPECT(second.Messages == Numbered(100));
}

// DebugConsolePanel keeps 0 as its "no listener" value
CDL_TEST_CASE(MainThreadLogSink, HandlesAreDistinctAndNeverZero, Unit)
{
	MainThreadLogSink sink(kCapacity);
	Received received;

	const LogListenerHandle first = sink.RegisterListener(received.Listener());
	const LogListenerHandle second = sink.RegisterListener(received.Listener());
	sink.UnregisterListener(first);
	const LogListenerHandle third = sink.RegisterListener(received.Listener());

	CDL_EXPECT_NE(first, 0u);
	CDL_EXPECT_NE(first, second);
	CDL_EXPECT_NE(first, third);
	CDL_EXPECT_NE(second, third);
}

CDL_TEST_CASE(MainThreadLogSink, UnregisteringRemovesOnlyThatListener, Unit)
{
	MainThreadLogSink sink(kCapacity);
	Received removed, kept;
	const LogListenerHandle handle = sink.RegisterListener(removed.Listener());
	sink.RegisterListener(kept.Listener());

	sink.UnregisterListener(handle);
	Write(sink, "after");
	sink.Dispatch();

	CDL_EXPECT_EQ(removed.Calls, 0);
	CDL_EXPECT_EQ(kept.Messages.size(), 1u);
}

CDL_TEST_CASE(MainThreadLogSink, UnregisteringAnUnknownHandleChangesNothing, Unit)
{
	MainThreadLogSink sink(kCapacity);
	Received received;
	const LogListenerHandle handle = sink.RegisterListener(received.Listener());

	sink.UnregisterListener(handle + 1);
	sink.UnregisterListener(0);
	Write(sink, "still heard");
	sink.Dispatch();

	CDL_EXPECT_EQ(received.Messages.size(), 1u);
}

// A write is all or nothing, so a message's parts are never half kept
CDL_TEST_CASE(MainThreadLogSink, WriteThatDoesNotFitIsDroppedWhole, Unit)
{
	MainThreadLogSink sink(4);
	Received received;
	sink.RegisterListener(received.Listener());

	WriteNumbered(sink, 3);
	const LogRecord tooMany[2]{};
	sink.Write(tooMany, 2);
	Write(sink, "fits");
	sink.Dispatch();

	CDL_EXPECT((received.Messages == std::vector<std::string>{ "0", "1", "2", "fits" }));
}

CDL_TEST_CASE(MainThreadLogSink, ReceivesWhatTheLoggerDrains, Unit)
{
	MainThreadLogSink& sink = Logger::AddSink(ScopedPtr<MainThreadLogSink>::Create(1024));
	std::vector<LogRecord> delivered;
	sink.RegisterListener([&](std::span<const LogRecord> records) { delivered.insert(delivered.end(), records.begin(), records.end()); });

	LoggerFixture logger;

	CDL_CORE_WARN(LogChannel::Command, "MainThreadLogSinkTests: marker {}", 42);
	CDL_CHECK(Logger::Flush());
	sink.Dispatch();

	const auto record = std::ranges::find_if(delivered, [](const LogRecord& candidate) {
		return std::string_view(candidate.Message) == "MainThreadLogSinkTests: marker 42";
		});
	CDL_CHECK(record != delivered.end());
	CDL_EXPECT_EQ(record->Level, LogLevel::Warn);
	CDL_EXPECT_EQ(record->Channel, LogChannel::Command);
}

// Dispatch racing the drain thread's Write, flushing often enough that the sink never has cause to drop anything
CDL_TEST_CASE(MainThreadLogSink, DispatchingWhileTheDrainThreadWritesLosesNothing, Stress)
{
	constexpr int kMessages = 8000;
	constexpr int kFlushInterval = 512;
	constexpr std::string_view kPrefix = "SinkStress ";

	MainThreadLogSink& sink = Logger::AddSink(ScopedPtr<MainThreadLogSink>::Create(1024));

	int received = 0, outOfOrder = 0;
	sink.RegisterListener([&](std::span<const LogRecord> records) {
		for (const LogRecord& record : records)
		{
			const std::string_view message = record.Message;
			if (!message.starts_with(kPrefix))
				continue;

			int sequence = -1;
			std::from_chars(message.data() + kPrefix.size(), message.data() + message.size(), sequence);
			if (sequence != received)
				++outOfOrder;
			++received;
		}
	});

	LoggerFixture logger;

	for (int i = 0; i < kMessages; ++i)
	{
		CDL_CORE_INFO(LogChannel::Thread, "SinkStress {}", i);
		sink.Dispatch();

		if ((i + 1) % kFlushInterval == 0)
		{
			CDL_CHECK(Logger::Flush());
			sink.Dispatch();
		}
	}

	CDL_CHECK(Logger::Flush());
	sink.Dispatch();

	CDL_EXPECT_EQ(outOfOrder, 0);
	CDL_CHECK_EQ(received, kMessages);
}
