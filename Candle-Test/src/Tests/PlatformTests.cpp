#include "TestFramework.h"
#include "TestHelpers.h"

#include <chrono>
#include <thread>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Unit;

CDL_TEST_CASE(Platform, TickIsMonotonic, Unit)
{
	Tick previous = Platform::GetTick();
	for (int i = 0; i < 10'000; ++i)
	{
		const Tick now = Platform::GetTick();
		CDL_CHECK(now.Value >= previous.Value);
		previous = now;
	}
	CDL_CHECK(Platform::GetStartTick().Value <= previous.Value);
}

CDL_TEST_CASE(Platform, ElapsedTimeMatchesASleep, Unit)
{
	const Tick start = Platform::GetTick();
	std::this_thread::sleep_for(std::chrono::milliseconds(20));
	const Tick end = Platform::GetTick();

	const uint64_t micros = Platform::ToMicroseconds(start, end);
	CDL_NOTE(std::format("slept 20 ms, measured {} us", micros));

	// Sleep never returns early; the upper bound is loose because the scheduler can oversleep a lot.
	CDL_EXPECT_GE(micros, 20'000u);
	CDL_EXPECT_LT(micros, 500'000u);

	// The two conversions have to agree.
	CDL_EXPECT_NEAR(Platform::ToSeconds(start, end), static_cast<double>(micros) / 1e6, 1e-5);
}

CDL_TEST_CASE(Platform, ZeroIntervalIsZero, Unit)
{
	const Tick tick = Platform::GetTick();
	CDL_CHECK_EQ(Platform::ToMicroseconds(tick, tick), 0u);
	CDL_CHECK_EQ(Platform::ToSeconds(tick, tick), 0.0);
}

CDL_TEST_CASE(Platform, UnixTimeTracksSystemClock, Unit)
{
	using namespace std::chrono;
	const int64_t systemMicros = duration_cast<microseconds>(system_clock::now().time_since_epoch()).count();
	const int64_t candleMicros = static_cast<int64_t>(Platform::ToUnixMicroseconds(Platform::GetTick()));

	// Anchored once at Platform::Init, so the two drift apart only by clock skew over the run.
	CDL_NOTE(std::format("Candle vs system clock: {} us", candleMicros - systemMicros));
	CDL_EXPECT_NEAR(candleMicros, systemMicros, 1'000'000);
}

CDL_TEST_CASE(Platform, ThreadIdIsStableAndDistinctPerThread, Unit)
{
	const uint32_t mine = Platform::GetCurrentThreadId();
	CDL_CHECK_EQ(Platform::GetCurrentThreadId(), mine);
	CDL_CHECK_NE(mine, 0u);

	uint32_t other = 0;
	std::jthread([&] { other = Platform::GetCurrentThreadId(); }).join();
	CDL_CHECK_NE(other, 0u);
	CDL_CHECK_NE(other, mine);
}
