// GPUResourcePool and GPUResourceRegistry with the GPU timeline faked by hand: a release keeps its payload until its
// retire value is reached, and its slot only returns to its own pool, through the packet, after that.

#include "TestFramework.h"
#include "TestHelpers.h"

#include <Candle/Render/FramePacket.h>
#include <Candle/Render/GPUResourcePool.h>
#include <Candle/Render/GPUResourceRegistry.h>

#include <cstdint>

using namespace Candle;
using Candle::Test::Type::Unit;

namespace {

	// Counts live payloads, so a test sees exactly when Collect destroys one.
	struct Payload
	{
		inline static int s_Alive = 0;

		explicit Payload(int value) : Value(value) { ++s_Alive; }
		Payload(Payload&& other) noexcept : Value(other.Value) { ++s_Alive; }
		~Payload() { --s_Alive; }

		int Value;
	};

	struct TestTag {};
	struct OtherTag {};
	using TestPool = GPUResourcePool<Payload, TestTag>;
	using OtherPool = GPUResourcePool<Payload, OtherTag>;

	constexpr uint32_t kCapacity = 8;

	// One frame of the protocol: this frame's releases retire at retireValue, the GPU is taken to have reached
	// completedValue, and whatever that frees comes back to the main thread.
	void RunFrame(GPUResourceRegistry& registry, uint64_t retireValue, uint64_t completedValue)
	{
		FramePacket packet;
		registry.MoveReleasedSlotsIntoPacket(packet);
		registry.RetireReleasedSlots(packet, retireValue);
		registry.FreeRetiredSlots(completedValue, packet);
		registry.ReturnRecycledSlotsToPools(packet);
	}

}

CDL_TEST_CASE(GPUResourcePool, InsertedPayloadIsReadableThroughItsHandle, Unit)
{
	Payload::s_Alive = 0;
	GPUResourceRegistry registry;
	TestPool pool(registry, kCapacity);

	const TestPool::Handle handle = pool.Insert(Payload(7));

	CDL_EXPECT(pool.IsAlive(handle));
	CDL_EXPECT_EQ(pool.Get(handle).Value, 7);
	CDL_EXPECT_EQ(Payload::s_Alive, 1);
}

// The handle dies on Release, but a frame in flight may still use the payload, so it lives until its value is reached.
CDL_TEST_CASE(GPUResourcePool, ReleaseKeepsThePayloadUntilTheGPUReachesItsRetireValue, Unit)
{
	Payload::s_Alive = 0;
	GPUResourceRegistry registry;
	TestPool pool(registry, kCapacity);
	const TestPool::Handle first = pool.Insert(Payload(1));
	const TestPool::Handle second = pool.Insert(Payload(2));

	pool.Release(first);
	CDL_EXPECT_FALSE(pool.IsAlive(first));
	RunFrame(registry, 1, 0);
	CDL_EXPECT_EQ(Payload::s_Alive, 2);

	pool.Release(second);
	RunFrame(registry, 2, 1);
	CDL_EXPECT_EQ(Payload::s_Alive, 1);

	RunFrame(registry, 3, 2);
	CDL_EXPECT_EQ(Payload::s_Alive, 0);
}

// A released slot still holds its payload until it retires, so it is only handed out again once it comes back.
CDL_TEST_CASE(GPUResourcePool, ReusesASlotOnlyOnceItIsRecycled, Unit)
{
	Payload::s_Alive = 0;
	GPUResourceRegistry registry;
	TestPool pool(registry, kCapacity);
	const TestPool::Handle released = pool.Insert(Payload(1));
	pool.Release(released);

	const TestPool::Handle beforeRecycle = pool.Insert(Payload(2));
	CDL_EXPECT_NE(beforeRecycle.Index, released.Index);

	RunFrame(registry, 1, 1);
	const TestPool::Handle afterRecycle = pool.Insert(Payload(3));
	CDL_EXPECT_EQ(afterRecycle.Index, released.Index);
	CDL_EXPECT_EQ(afterRecycle.Generation, released.Generation + 1);
	CDL_EXPECT(pool.IsAlive(afterRecycle));
	CDL_EXPECT_FALSE(pool.IsAlive(released));
}

// Both pools hand out index 0 first, so a release or a recycled slot routed to the wrong pool shows up here.
CDL_TEST_CASE(GPUResourcePool, PoolsShareOnePacketWithoutMixingSlots, Unit)
{
	Payload::s_Alive = 0;
	GPUResourceRegistry registry;
	TestPool pool(registry, kCapacity);
	OtherPool other(registry, kCapacity);
	const TestPool::Handle mine = pool.Insert(Payload(1));
	const OtherPool::Handle theirs = other.Insert(Payload(2));
	const OtherPool::Handle kept = other.Insert(Payload(3));

	other.Release(theirs);
	RunFrame(registry, 1, 1);

	CDL_EXPECT_EQ(Payload::s_Alive, 2);
	CDL_EXPECT_EQ(pool.Get(mine).Value, 1);
	CDL_EXPECT_EQ(other.Get(kept).Value, 3);
	CDL_EXPECT_EQ(other.Insert(Payload(4)).Index, theirs.Index);
	CDL_EXPECT_NE(pool.Insert(Payload(5)).Index, mine.Index);
}
