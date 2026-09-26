#include "TestFramework.h"
#include "TestHelpers.h"

#include <Candle/Core/Threading/MPSCRingBuffer.h>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Unit;
using Candle::Test::Type::Stress;

CDL_TEST_CASE(MPSCRingBuffer, EmptyPopReturnsNothing, Unit)
{
	MPSCRingBuffer<int> ring(8);
	CDL_CHECK_FALSE(ring.Pop().has_value());
	CDL_CHECK_EQ(ring.Capacity(), 8u);
	CDL_CHECK_EQ(ring.DroppedCount(), 0u);
}

CDL_TEST_CASE(MPSCRingBuffer, PopsInPushOrder, Unit)
{
	MPSCRingBuffer<int> ring(8);
	for (int i = 0; i < 5; ++i)
		CDL_CHECK(ring.TryPush(i));

	for (int i = 0; i < 5; ++i)
	{
		const std::optional<int> value = ring.Pop();
		CDL_CHECK(value.has_value());
		CDL_CHECK_EQ(*value, i);
	}
	CDL_CHECK_FALSE(ring.Pop().has_value());
}

CDL_TEST_CASE(MPSCRingBuffer, FullRingDropsAndCounts, Unit)
{
	MPSCRingBuffer<int> ring(4);
	for (int i = 0; i < 4; ++i)
		CDL_CHECK(ring.TryPush(i));

	CDL_CHECK_FALSE(ring.TryPush(100));
	CDL_CHECK_FALSE(ring.TryPush(101));
	CDL_CHECK_EQ(ring.DroppedCount(), 2u);

	// The dropped values must not have overwritten anything.
	CDL_CHECK_EQ(*ring.Pop(), 0);

	// One slot free again, so one push fits.
	CDL_CHECK(ring.TryPush(4));
	CDL_CHECK_FALSE(ring.TryPush(5));
	CDL_CHECK_EQ(ring.DroppedCount(), 3u);
}

CDL_TEST_CASE(MPSCRingBuffer, WrapsAroundManyTimes, Unit)
{
	MPSCRingBuffer<int> ring(8);
	int next = 0;
	int expected = 0;

	// Three in, three out, with a capacity of 8, so the indices wrap roughly 375 times.
	for (int round = 0; round < 1000; ++round)
	{
		for (int i = 0; i < 3; ++i)
			CDL_CHECK(ring.TryPush(next++));

		for (int i = 0; i < 3; ++i)
			CDL_CHECK_EQ(*ring.Pop(), expected++);
	}
	CDL_CHECK_EQ(ring.DroppedCount(), 0u);
}

CDL_TEST_CASE(MPSCRingBuffer, PopBatchRespectsMaxCount, Unit)
{
	MPSCRingBuffer<int> ring(8);
	for (int i = 0; i < 5; ++i)
		ring.TryPush(i);

	int out[10] = {};
	CDL_CHECK_EQ(ring.PopBatch(out, 3), 3u);
	CDL_EXPECT_EQ(out[0], 0);
	CDL_EXPECT_EQ(out[1], 1);
	CDL_EXPECT_EQ(out[2], 2);

	CDL_CHECK_EQ(ring.PopBatch(out, 10), 2u);
	CDL_EXPECT_EQ(out[0], 3);
	CDL_EXPECT_EQ(out[1], 4);

	CDL_CHECK_EQ(ring.PopBatch(out, 10), 0u);
}

CDL_TEST_CASE(MPSCRingBuffer, PopBatchFreesSlotsForProducers, Unit)
{
	MPSCRingBuffer<int> ring(4);
	for (int i = 0; i < 4; ++i)
		ring.TryPush(i);

	int out[4] = {};
	CDL_CHECK_EQ(ring.PopBatch(out, 4), 4u);

	for (int i = 0; i < 4; ++i)
		CDL_CHECK(ring.TryPush(10 + i));
	CDL_CHECK_EQ(*ring.Pop(), 10);
}

namespace {

	// Trivially copyable, so it can go through the ring.
	struct Message
	{
		uint32_t Producer = 0;
		uint32_t Sequence = 0;
	};

}

CDL_TEST_CASE(MPSCRingBuffer, ManyProducersOneConsumerLoseNothing, Stress)
{
	// A small ring forces constant wraparound and full-ring retries. Each producer's sequence must
	// arrive exactly once and in order; interleaving between producers is unconstrained.
	constexpr uint32_t perProducer = 50'000;
	const int producerCount = StressThreadCount() - 1;
	const uint64_t total = static_cast<uint64_t>(producerCount) * perProducer;

	MPSCRingBuffer<Message> ring(256);
	std::vector<uint32_t> nextExpected(producerCount, 0);
	uint64_t received = 0;
	uint64_t outOfOrder = 0;
	std::atomic<int> producersDone = 0;

	// Thread 0 is the single consumer; the rest are producers.
	RunConcurrently(producerCount + 1, [&](int thread) {
		if (thread == 0)
		{
			// Exits once a pass that started after every producer finished comes back empty, so a lost
			// message fails the count check below instead of hanging the suite.
			Message batch[64];
			while (true)
			{
				const bool allDone = producersDone.load(std::memory_order_acquire) == producerCount;
				const size_t count = ring.PopBatch(batch, std::size(batch));
				for (size_t i = 0; i < count; ++i)
				{
					uint32_t& expected = nextExpected[batch[i].Producer];
					if (batch[i].Sequence != expected)
						++outOfOrder;
					expected = batch[i].Sequence + 1;
				}
				received += count;

				if (count == 0 && allDone)
					break;
			}
			return;
		}

		const uint32_t producer = static_cast<uint32_t>(thread - 1);
		for (uint32_t sequence = 0; sequence < perProducer; ++sequence)
		{
			while (!ring.TryPush(Message{ producer, sequence }))
				CDL_THREAD_PAUSE();
		}
		producersDone.fetch_add(1, std::memory_order_release);
	});

	CDL_NOTE(std::format("{} producers x {} messages, {} full-ring retries", producerCount, perProducer, ring.DroppedCount()));
	CDL_CHECK_EQ(received, total);
	CDL_CHECK_EQ(outOfOrder, 0u);
	for (int producer = 0; producer < producerCount; ++producer)
		CDL_EXPECT_EQ(nextExpected[producer], perProducer);
	CDL_CHECK_FALSE(ring.Pop().has_value());
}
