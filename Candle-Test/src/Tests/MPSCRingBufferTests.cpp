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

CDL_TEST_CASE(MPSCRingBuffer, PushNIsAllOrNothing, Unit)
{
	MPSCRingBuffer<int> ring(8);
	const int group[] = { 10, 11, 12, 13 };
	CDL_CHECK(ring.TryPushN(group, 4));
	CDL_CHECK(ring.TryPush(14));

	// Three slots free: a group of four must be refused whole, not partly written.
	CDL_CHECK_FALSE(ring.TryPushN(group, 4));
	CDL_CHECK_EQ(ring.DroppedCount(), 4u);

	CDL_CHECK(ring.TryPushN(group, 3));
	CDL_CHECK_FALSE(ring.TryPush(99));

	int out[8] = {};
	CDL_CHECK_EQ(ring.PopBatch(out, 8), 8u);
	const int expected[] = { 10, 11, 12, 13, 14, 10, 11, 12 };
	for (size_t i = 0; i < std::size(expected); ++i)
		CDL_EXPECT_EQ(out[i], expected[i]);
}

CDL_TEST_CASE(MPSCRingBuffer, PushNWrapsAcrossTheEnd, Unit)
{
	MPSCRingBuffer<int> ring(8);
	int out[8] = {};
	for (int i = 0; i < 6; ++i)
		CDL_CHECK(ring.TryPush(i));
	CDL_CHECK_EQ(ring.PopBatch(out, 8), 6u);

	// Lands in slots 6, 7, 0 and 1.
	const int group[] = { 20, 21, 22, 23 };
	CDL_CHECK(ring.TryPushN(group, 4));

	CDL_CHECK_EQ(ring.PopBatch(out, 8), 4u);
	for (size_t i = 0; i < std::size(group); ++i)
		CDL_EXPECT_EQ(out[i], group[i]);
}

namespace {

	// Trivially copyable, so it can go through the ring.
	struct Message
	{
		uint32_t Producer = 0;
		uint32_t Sequence = 0;
	};

	// One item of a TryPushN group; Remaining counts down to 0 on the group's last item.
	struct GroupItem
	{
		uint32_t Producer = 0;
		uint32_t Group = 0;
		uint32_t Remaining = 0;
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

CDL_TEST_CASE(MPSCRingBuffer, PushNGroupsStayUnbrokenUnderContention, Stress)
{
	// Every producer pushes groups of 1-7 items into a small ring at once. Each group must come out with
	// nothing from another producer inside it, and each producer's groups must come out in order.
	constexpr uint32_t groupsPerProducer = 20'000;
	constexpr uint32_t maxGroupSize = 7;
	const int producerCount = StressThreadCount() - 1;

	uint64_t total = 0;
	for (int producer = 0; producer < producerCount; ++producer)
		for (uint32_t group = 0; group < groupsPerProducer; ++group)
			total += 1 + (group + producer) % maxGroupSize;

	MPSCRingBuffer<GroupItem> ring(256);
	std::vector<uint32_t> nextGroup(producerCount, 0);
	uint64_t received = 0;
	uint64_t broken = 0;
	uint64_t outOfOrder = 0;
	std::atomic<int> producersDone = 0;

	// Thread 0 is the single consumer; the rest are producers.
	RunConcurrently(producerCount + 1, [&](int thread) {
		if (thread == 0)
		{
			GroupItem batch[64];
			GroupItem previous;
			bool insideGroup = false;
			while (true)
			{
				const bool allDone = producersDone.load(std::memory_order_acquire) == producerCount;
				const size_t count = ring.PopBatch(batch, std::size(batch));
				for (size_t i = 0; i < count; ++i)
				{
					const GroupItem& item = batch[i];
					if (insideGroup && (item.Producer != previous.Producer || item.Group != previous.Group || item.Remaining + 1 != previous.Remaining))
						++broken;

					if (item.Remaining == 0 && item.Group != nextGroup[item.Producer]++)
						++outOfOrder;

					insideGroup = item.Remaining > 0;
					previous = item;
				}
				received += count;

				if (count == 0 && allDone)
					break;
			}
			return;
		}

		const uint32_t producer = static_cast<uint32_t>(thread - 1);
		GroupItem items[maxGroupSize];
		for (uint32_t group = 0; group < groupsPerProducer; ++group)
		{
			const uint32_t size = 1 + (group + producer) % maxGroupSize;
			for (uint32_t i = 0; i < size; ++i)
				items[i] = GroupItem{ producer, group, size - 1 - i };

			while (!ring.TryPushN(items, size))
				CDL_THREAD_PAUSE();
		}
		producersDone.fetch_add(1, std::memory_order_release);
	});

	CDL_NOTE(std::format("{} producers x {} groups, {} items refused while full", producerCount, groupsPerProducer, ring.DroppedCount()));
	CDL_CHECK_EQ(received, total);
	CDL_CHECK_EQ(broken, 0u);
	CDL_CHECK_EQ(outOfOrder, 0u);
	for (int producer = 0; producer < producerCount; ++producer)
		CDL_EXPECT_EQ(nextGroup[producer], groupsPerProducer);
	CDL_CHECK_FALSE(ring.Pop().has_value());
}
