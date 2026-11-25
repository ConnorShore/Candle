// ArenaAllocator bumps through one buffer and gets all of it back at once on Reset. The frame graph keeps every pass's data and
// execute captures in one, so each allocation must be aligned for its type and never overlap another, and Reset must hand the
// same memory out again rather than creep through the buffer frame by frame.

#include "TestFramework.h"

#include <Candle/Core/Memory/Collections/ArenaAllocator.h>

#include <cstdint>

using namespace Candle;
using Candle::Test::Type::Unit;

namespace {

	struct Point
	{
		int32_t X;
		int32_t Y;
	};

	// Wider than the 16 bytes new[] aligns the buffer to, so only padding worked out from the real address lines it up
	struct alignas(64) Overaligned
	{
		uint8_t Bytes[64];
	};

	bool IsAligned(const void* pointer, size_t alignment)
	{
		return reinterpret_cast<uintptr_t>(pointer) % alignment == 0;
	}

}

CDL_TEST_CASE(ArenaAllocator, ConstructsWithTheArgumentsItIsGiven, Unit)
{
	ArenaAllocator arena(256);

	const Point* point = arena.New<Point>(3, 4);
	CDL_CHECK(point != nullptr);
	CDL_EXPECT_EQ(point->X, 3);
	CDL_EXPECT_EQ(point->Y, 4);
}

// Reset leaves the bytes where they were, so a value made with no arguments has to be zeroed rather than inherit them
CDL_TEST_CASE(ArenaAllocator, NewWithNoArgumentsValueInitialises, Unit)
{
	ArenaAllocator arena(64);
	arena.New<uint64_t>(~uint64_t{ 0 });
	arena.Reset();

	const uint64_t* value = arena.New<uint64_t>();
	CDL_CHECK(value != nullptr);
	CDL_EXPECT_EQ(*value, uint64_t{ 0 });
}

CDL_TEST_CASE(ArenaAllocator, AllocationsNeverOverlap, Unit)
{
	constexpr uint32_t kCount = 64;
	ArenaAllocator arena(kCount * sizeof(uint32_t) * 2);

	uint32_t* values[kCount];
	for (uint32_t i = 0; i < kCount; ++i)
		values[i] = arena.New<uint32_t>(i);

	for (uint32_t i = 0; i < kCount; ++i)
	{
		CDL_CHECK(values[i] != nullptr);
		CDL_EXPECT_EQ(*values[i], i);
	}
}

// Single bytes in between, so every allocation after one starts off misaligned and has to be padded up
CDL_TEST_CASE(ArenaAllocator, EachAllocationIsAlignedForItsType, Unit)
{
	ArenaAllocator arena(1024);

	arena.New<uint8_t>(uint8_t{ 1 });
	const uint64_t* wide = arena.New<uint64_t>(uint64_t{ 2 });
	arena.New<uint8_t>(uint8_t{ 3 });
	const Overaligned* overaligned = arena.New<Overaligned>();
	arena.New<uint8_t>(uint8_t{ 4 });
	const uint32_t* narrow = arena.New<uint32_t>(5u);

	CDL_CHECK(wide && overaligned && narrow);
	CDL_EXPECT(IsAligned(wide, alignof(uint64_t)));
	CDL_EXPECT(IsAligned(overaligned, alignof(Overaligned)));
	CDL_EXPECT(IsAligned(narrow, alignof(uint32_t)));
	CDL_EXPECT_EQ(*wide, uint64_t{ 2 });
	CDL_EXPECT_EQ(*narrow, 5u);
}

CDL_TEST_CASE(ArenaAllocator, FillsToExactlyItsCapacity, Unit)
{
	constexpr uint32_t kCount = 4;
	ArenaAllocator arena(kCount * sizeof(uint64_t));

	for (uint32_t i = 0; i < kCount; ++i)
		CDL_EXPECT(arena.New<uint64_t>(i) != nullptr);
}

// The arena is full before the Reset, so the allocation after it only fits if Reset gave the whole buffer back
CDL_TEST_CASE(ArenaAllocator, ResetHandsOutTheSameMemoryAgain, Unit)
{
	ArenaAllocator arena(2 * sizeof(Point));
	const Point* first = arena.New<Point>(1, 2);
	arena.New<Point>(3, 4);

	arena.Reset();
	const Point* again = arena.New<Point>(5, 6);

	CDL_CHECK(again != nullptr);
	CDL_EXPECT(again == first);
	CDL_EXPECT_EQ(again->X, 5);
}
