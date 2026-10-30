// GPUResourcePool and GPUResourceRegistry with the GPU timeline faked by hand: a release keeps its payload until its
// retire value is reached, and its slot only returns to its own pool, through the packet, after that.

#include "TestFramework.h"
#include "TestHelpers.h"

#include <Candle/Render/FramePacket.h>
#include <Candle/Render/Resources/GPUResourcePool.h>
#include <Candle/Render/Resources/GPUResourceRegistry.h>

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

CDL_TEST_CASE(GPUResourcePool, HandlesItNeverIssuedAreNotAlive, Unit)
{
	GPUResourceRegistry registry;
	TestPool pool(registry, kCapacity);

	CDL_EXPECT_FALSE(pool.IsAlive(TestPool::Handle{}));
	CDL_EXPECT_FALSE(pool.IsAlive(TestPool::Handle{ .Index = 0, .Generation = 0 }));	// In range, but never filled
}

// A packet carries the releases made before it was begun; one made after waits for the next packet.
CDL_TEST_CASE(GPUResourcePool, AReleaseRidesTheNextPacketBuilt, Unit)
{
	GPUResourceRegistry registry;
	TestPool pool(registry, kCapacity);
	const TestPool::Handle handle = pool.Insert(Payload(1));

	FramePacket current;
	registry.MoveReleasedSlotsIntoPacket(current);
	pool.Release(handle);
	CDL_EXPECT(current.ReleasedSlots.empty());

	FramePacket next;
	registry.MoveReleasedSlotsIntoPacket(next);
	CDL_CHECK_EQ(next.ReleasedSlots.size(), 1u);
	CDL_EXPECT_EQ(next.ReleasedSlots[0].Index, handle.Index);
	CDL_EXPECT_EQ(next.ReleasedSlots[0].Generation, handle.Generation);
}

// Packets are reused, so one comes back still holding what it carried last time. That must ride neither it nor a later
// packet, or the slot would retire twice.
CDL_TEST_CASE(GPUResourcePool, BuildingAPacketDropsTheReleasesItCarriedLastTime, Unit)
{
	GPUResourceRegistry registry;
	TestPool pool(registry, kCapacity);
	pool.Release(pool.Insert(Payload(1)));

	FramePacket packet;
	registry.MoveReleasedSlotsIntoPacket(packet);
	CDL_CHECK_EQ(packet.ReleasedSlots.size(), 1u);

	registry.MoveReleasedSlotsIntoPacket(packet);
	CDL_EXPECT(packet.ReleasedSlots.empty());

	FramePacket later;
	registry.MoveReleasedSlotsIntoPacket(later);
	CDL_EXPECT(later.ReleasedSlots.empty());
}

// A slot returned twice would sit on the free list twice, and two live handles would share it.
CDL_TEST_CASE(GPUResourcePool, ReturningRecycledSlotsEmptiesThePacket, Unit)
{
	GPUResourceRegistry registry;
	TestPool pool(registry, kCapacity);
	pool.Release(pool.Insert(Payload(1)));

	FramePacket packet;
	registry.MoveReleasedSlotsIntoPacket(packet);
	registry.RetireReleasedSlots(packet, 1);
	registry.FreeRetiredSlots(1, packet);
	CDL_CHECK_EQ(packet.RecycledSlots.size(), 1u);

	registry.ReturnRecycledSlotsToPools(packet);
	CDL_EXPECT(packet.RecycledSlots.empty());
	registry.ReturnRecycledSlotsToPools(packet);	// The packet comes round again

	const TestPool::Handle first = pool.Insert(Payload(2));
	const TestPool::Handle second = pool.Insert(Payload(3));
	CDL_EXPECT_NE(first.Index, second.Index);
}

// Retire values rise, so freeing stops at the first slot the GPU hasn't reached, and a slot freed once is gone.
CDL_TEST_CASE(GPUResourcePool, FreesExactlyTheSlotsTheGPUHasReached, Unit)
{
	Payload::s_Alive = 0;
	GPUResourceRegistry registry;
	TestPool pool(registry, kCapacity);
	FramePacket packet;
	for (uint64_t retireValue = 1; retireValue <= 3; ++retireValue)
	{
		pool.Release(pool.Insert(Payload(static_cast<int>(retireValue))));
		registry.MoveReleasedSlotsIntoPacket(packet);
		registry.RetireReleasedSlots(packet, retireValue);
	}

	registry.FreeRetiredSlots(2, packet);
	CDL_EXPECT_EQ(packet.RecycledSlots.size(), 2u);
	CDL_EXPECT_EQ(Payload::s_Alive, 1);

	registry.FreeRetiredSlots(2, packet);
	CDL_EXPECT_EQ(packet.RecycledSlots.size(), 2u);
	CDL_EXPECT_EQ(Payload::s_Alive, 1);
}

// The GPU is idle by the time a pool is destroyed, so it frees whatever it still holds: live, retiring or just released.
CDL_TEST_CASE(GPUResourcePool, DestroyingThePoolFreesEveryPayloadItHolds, Unit)
{
	Payload::s_Alive = 0;
	GPUResourceRegistry registry;
	{
		TestPool pool(registry, kCapacity);
		pool.Insert(Payload(1));

		pool.Release(pool.Insert(Payload(2)));
		FramePacket packet;
		registry.MoveReleasedSlotsIntoPacket(packet);
		registry.RetireReleasedSlots(packet, 1);

		pool.Release(pool.Insert(Payload(3)));
		CDL_CHECK_EQ(Payload::s_Alive, 3);
	}
	CDL_EXPECT_EQ(Payload::s_Alive, 0);
}
