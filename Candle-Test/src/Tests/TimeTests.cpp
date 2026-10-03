#include "TestFramework.h"
#include "TestHelpers.h"

#include <charconv>
#include <string_view>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Unit;

namespace {

	const LogRecord* FindPrefix(const std::vector<LogRecord>& records, std::string_view prefix)
	{
		for (const LogRecord& record : records)
		{
			if (std::string_view(record.Message).starts_with(prefix))
				return &record;
		}
		return nullptr;
	}

}

CDL_TEST_CASE(Time, TimeStepDefaultsToZero, Unit)
{
	const TimeStep step;
	CDL_CHECK_EQ(step.GetSeconds(), 0.0f);
	CDL_CHECK_EQ(step.GetMilliseconds(), 0.0f);
}

CDL_TEST_CASE(Time, TimeStepConvertsUnits, Unit)
{
	const TimeStep step(0.25f);
	CDL_EXPECT_EQ(step.GetSeconds(), 0.25f);
	CDL_EXPECT_NEAR(step.GetMilliseconds(), 250.0f, 1e-4f);
	CDL_EXPECT_EQ(static_cast<float>(step), 0.25f);
}

CDL_TEST_CASE(Time, TimerStartsAtConstruction, Unit)
{
	// A timer that was never started would measure from the clock's epoch, i.e. the machine's uptime.
	const Timer timer;
	CDL_EXPECT_GE(timer.ElapsedSeconds(), 0.0);
	CDL_EXPECT_LT(timer.ElapsedSeconds(), 1.0);
}

CDL_TEST_CASE(Time, TimerMeasuresASleep, Unit)
{
	const Timer timer;
	Platform::SleepCurrentThread(20);

	const double ms = timer.ElapsedMilliseconds();
	const double seconds = timer.ElapsedSeconds();
	CDL_NOTE(std::format("slept 20 ms, measured {:.3f} ms", ms));

	// Sleep never returns early; the upper bound is loose because the scheduler can oversleep a lot.
	CDL_EXPECT_GE(ms, 20.0);
	CDL_EXPECT_LT(ms, 500.0);

	// Read one after the other, so seconds can only be the later (larger) reading.
	CDL_EXPECT_GE(seconds * 1000.0, ms);
	CDL_EXPECT_NEAR(seconds * 1000.0, ms, 5.0);
}

CDL_TEST_CASE(Time, TimerElapsedNeverDecreases, Unit)
{
	const Timer timer;
	double previous = timer.ElapsedSeconds();
	for (int i = 0; i < 10'000; ++i)
	{
		const double now = timer.ElapsedSeconds();
		CDL_CHECK(now >= previous);
		previous = now;
	}
}

CDL_TEST_CASE(Time, TimerResetRestartsTheInterval, Unit)
{
	Timer timer;
	Platform::SleepCurrentThread(20);
	const double beforeReset = timer.ElapsedMilliseconds();

	timer.Reset();
	const double afterReset = timer.ElapsedMilliseconds();

	CDL_EXPECT_GE(beforeReset, 20.0);
	CDL_EXPECT_LT(afterReset, beforeReset);
}

CDL_TEST_CASE(Time, ScopedTimerLogsOnDestruction, Unit)
{
	LoggerFixture logger;
	{
		ScopedTimer timer("TimeTests marker", LogChannel::Physics);
		Platform::SleepCurrentThread(5);
	}
	CDL_CHECK(Logger::Flush());

	constexpr std::string_view prefix = "ScopedTimer [TimeTests marker] elapsed time: ";
	const LogRecord* record = FindPrefix(logger.Records, prefix);
	CDL_CHECK(record != nullptr);
	CDL_EXPECT_EQ(record->Channel, LogChannel::Physics);
	CDL_EXPECT_EQ(record->Level, LogLevel::Info);
	CDL_EXPECT_EQ(record->Type, 0);

	const std::string_view message(record->Message);
	double ms = -1.0;
	const auto [end, error] = std::from_chars(message.data() + prefix.size(), message.data() + message.size(), ms);
	CDL_CHECK(error == std::errc{});
	CDL_EXPECT(std::string_view(end) == " ms");
	CDL_EXPECT_GE(ms, 5.0);
	CDL_EXPECT_LT(ms, 500.0);
}

CDL_TEST_CASE(Time, ScopedTimerLogsNothingUntilDestroyed, Unit)
{
	LoggerFixture logger;
	ScopedTimer timer("TimeTests still running");
	CDL_CHECK(Logger::Flush());
	CDL_EXPECT(FindPrefix(logger.Records, "ScopedTimer [TimeTests still running]") == nullptr);
}
