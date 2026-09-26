// Measurement-only baselines. These never fail on timing; switch a REPORT to a BUDGET once a number
// has been stable across a few runs on the target machine.

#include "TestFramework.h"
#include "TestHelpers.h"

#include <Candle/Core/Threading/MPSCRingBuffer.h>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Unit;

CDL_TEST_CASE(Benchmark, LoggerPushPath, Unit)
{
	// Flushing every iteration keeps the ring from filling, so this times format + push + drain, never drops.
	LoggerFixture logger;
	CDL_BENCH_REPORT("256 CDL_CORE_INFO + Flush", 50, {
		for (int i = 0; i < 256; ++i)
			CDL_CORE_INFO(LogChannel::Application, "Benchmark record {} of {}", i, 256);
		Logger::Flush();
	});
}

CDL_TEST_CASE(Benchmark, RingBufferPushPopSingleThread, Unit)
{
	MPSCRingBuffer<uint64_t> ring(1024);
	uint64_t sink = 0;
	CDL_BENCH_REPORT("1024 TryPush + 1024 Pop", 200, {
		for (uint64_t i = 0; i < 1024; ++i)
			ring.TryPush(i);
		while (std::optional<uint64_t> value = ring.Pop())
			sink += *value;
	});
	CDL_CHECK_NE(sink, 0u);
}
