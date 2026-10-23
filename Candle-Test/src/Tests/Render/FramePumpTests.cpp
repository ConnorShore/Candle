// FramePump's hand-off of packets to the reader and back, the writer's wait once every packet is out, and Close, which
// is how shutdown frees a render thread waiting on it.

#include "TestFramework.h"
#include "TestHelpers.h"

#include <Candle/Core/Threading/Thread.h>
#include <Candle/Render/FramePump.h>

#include <atomic>
#include <cstdint>
#include <vector>

using namespace Candle;
using Candle::Test::Type::Stress;
using Candle::Test::Type::Unit;

CDL_TEST_CASE(FramePump, ReaderGetsThePacketTheWriterSubmitted, Unit)
{
	FramePump pump;
	FramePacket& written = pump.AquireForWriting();
	written.Time = 1.5;
	pump.Submit(written);

	FramePacket* read = pump.AquireForReading();

	CDL_CHECK(read != nullptr);
	CDL_EXPECT(read == &written);
	CDL_EXPECT_EQ(read->Time, 1.5);
}

// Shutdown doesn't draw packets the render thread never got to, so Close wins over them.
CDL_TEST_CASE(FramePump, ReadingAfterCloseReturnsNullEvenWithPacketsUnread, Unit)
{
	FramePump pump;
	pump.Submit(pump.AquireForWriting());
	pump.Close();

	CDL_EXPECT(pump.AquireForReading() == nullptr);
}

// Fails by hanging rather than by assertion: without Close waking it, the reader waits forever.
CDL_TEST_CASE(FramePump, CloseWakesAReaderWaitingForAPacket, Unit)
{
	FramePump pump;
	std::atomic<bool> readNull = false;

	Thread reader("FramePumpReader");
	reader.Start([&] { readNull = pump.AquireForReading() == nullptr; });

	// Gives the reader time to start waiting; if Close lands first the read returns at once, which passes too
	Platform::SleepCurrentThread(10);
	pump.Close();
	reader.Join();

	CDL_EXPECT(readNull);
}

// Packets alternate slots, numbered in submission order, and the reader takes them in that order.
CDL_TEST_CASE(FramePump, HandsOutPacketsInTurnAndNumbersThem, Unit)
{
	FramePump pump;
	FramePacket& first = pump.AquireForWriting();
	CDL_EXPECT_EQ(first.FrameIndex, 0u);
	pump.Submit(first);
	FramePacket& second = pump.AquireForWriting();
	CDL_EXPECT_EQ(second.FrameIndex, 1u);
	CDL_EXPECT(&second != &first);
	pump.Submit(second);

	FramePacket* read = pump.AquireForReading();
	CDL_CHECK(read == &first);
	pump.Release(*read);

	FramePacket& third = pump.AquireForWriting();
	CDL_EXPECT(&third == &first);
	CDL_EXPECT_EQ(third.FrameIndex, 2u);
	CDL_EXPECT(pump.AquireForReading() == &second);
}

// The writer runs at most kFramePacketCount packets ahead, so the next acquire waits for the reader to give one back.
// The reader only releases after its sleep, so a writer that returned early sees the flag still clear.
CDL_TEST_CASE(FramePump, WriterWaitsForAReleaseOnceEveryPacketIsOut, Unit)
{
	FramePump pump;
	for (uint32_t i = 0; i < kFramePacketCount; ++i)
		pump.Submit(pump.AquireForWriting());

	std::atomic<bool> released = false;
	Thread reader("FramePumpReader");
	reader.Start([&] {
		Platform::SleepCurrentThread(10);
		FramePacket* packet = pump.AquireForReading();
		released = true;
		pump.Release(*packet);
	});

	pump.AquireForWriting();
	const bool waitedForTheRelease = released;
	reader.Join();

	CDL_EXPECT(waitedForTheRelease);
}

// Writer and reader flat out, so every hand-off races both ways: the reader must see each packet once, in order, with
// what the writer wrote, and the writer must see what the reader wrote back into it. Fails by hanging on a lost wakeup.
CDL_TEST_CASE(FramePump, EveryPacketCrossesOnceInOrderBothWays, Stress)
{
	constexpr uint64_t kPackets = 20'000;
	FramePump pump;

	// Reader thread only, until it is joined
	std::vector<uint64_t> readIndices;
	readIndices.reserve(kPackets);
	uint64_t readTimeMismatches = 0;

	Thread reader("FramePumpReader");
	reader.Start([&] {
		for (uint64_t i = 0; i < kPackets; ++i)
		{
			FramePacket* packet = pump.AquireForReading();
			if (!packet)
				return;

			readIndices.push_back(packet->FrameIndex);
			if (packet->Time != static_cast<double>(packet->FrameIndex))
				++readTimeMismatches;

			// The render thread's one write, as RecycledSlots is in the real protocol
			packet->RecycledSlots.assign(1, GPUSlotRef{ .PoolID = 0, .Index = static_cast<uint32_t>(packet->FrameIndex), .Generation = 0 });
			pump.Release(*packet);
		}
	});

	uint64_t indexMismatches = 0, replyMismatches = 0;
	for (uint64_t i = 0; i < kPackets; ++i)
	{
		FramePacket& packet = pump.AquireForWriting();
		if (packet.FrameIndex != i)
			++indexMismatches;

		// The slot was last read kFramePacketCount frames ago, and the writer can't have it back before that release
		const bool replied = packet.RecycledSlots.size() == 1 && packet.RecycledSlots[0].Index == i - kFramePacketCount;
		if (i >= kFramePacketCount && !replied)
			++replyMismatches;

		packet.Time = static_cast<double>(packet.FrameIndex);
		pump.Submit(packet);
	}
	reader.Join();

	CDL_EXPECT_EQ(indexMismatches, 0u);
	CDL_EXPECT_EQ(replyMismatches, 0u);
	CDL_EXPECT_EQ(readTimeMismatches, 0u);
	CDL_CHECK_EQ(readIndices.size(), kPackets);
	uint64_t outOfOrder = 0;
	for (uint64_t i = 0; i < kPackets; ++i)
		outOfOrder += readIndices[i] != i;
	CDL_EXPECT_EQ(outOfOrder, 0u);
}
