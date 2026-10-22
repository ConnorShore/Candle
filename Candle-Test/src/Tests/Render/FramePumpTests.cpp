// FramePump's hand-off of packets to the reader, and Close, which is how shutdown frees a render thread waiting on it.

#include "TestFramework.h"
#include "TestHelpers.h"

#include <Candle/Core/Threading/Thread.h>
#include <Candle/Render/FramePump.h>

#include <atomic>

using namespace Candle;
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
