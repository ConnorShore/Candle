// One thread's frame timings, which both frame panels read their busy time from.

#include "TestFramework.h"
#include "TestHelpers.h"

#include <Candle/Core/FrameTimings.h>

using namespace Candle;
using Candle::Test::Type::Unit;

CDL_TEST_CASE(FrameTimings, BusyIsTheTotalLessThePumpWait, Unit)
{
	CDL_EXPECT_EQ(ThreadFrameTimings{}.BusyMs(), 0.0f);
	CDL_EXPECT_EQ((ThreadFrameTimings{ .TotalMs = 16.0f, .PumpWaitMs = 6.0f }).BusyMs(), 10.0f);
	CDL_EXPECT_EQ((ThreadFrameTimings{ .TotalMs = 16.0f }).BusyMs(), 16.0f);
}
