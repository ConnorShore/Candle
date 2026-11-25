// Memory:: is where global new/delete and the SDL, ImGui, stb and VMA hooks all land, so the profiler sees every allocation.
// Each library was written against C's semantics: realloc keeps the contents and takes null and zero, calloc zeroes, and
// the aligned pair keeps the alignment it was asked for.

#include "TestFramework.h"

#include <Candle/Core/Memory/GlobalAllocation.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

using namespace Candle;
using Candle::Test::Type::Unit;

namespace {

	bool IsAligned(const void* pointer, size_t alignment)
	{
		return reinterpret_cast<uintptr_t>(pointer) % alignment == 0;
	}

	// Wider than the 16 bytes malloc gives, so only the aligned new overload lines it up
	struct alignas(256) Overaligned
	{
		uint8_t Bytes[256];
	};

}

CDL_TEST_CASE(GlobalAllocation, AllocateAlignedHonoursEachAlignment, Unit)
{
	constexpr size_t kAlignments[] = { 16, 64, 256, 4096 };
	for (size_t alignment : kAlignments)
	{
		void* ptr = Memory::AllocateAligned(100, alignment);
		CDL_CHECK(ptr != nullptr);
		CDL_EXPECT(IsAligned(ptr, alignment));
		std::memset(ptr, 0xAB, 100);
		Memory::FreeAligned(ptr);
	}
}

// stb grows its buffers with realloc, so what was written before has to survive the block moving
CDL_TEST_CASE(GlobalAllocation, ReallocateKeepsTheContents, Unit)
{
	constexpr size_t kSmall = 32;
	auto* bytes = static_cast<uint8_t*>(Memory::Allocate(kSmall));
	CDL_CHECK(bytes != nullptr);
	for (size_t i = 0; i < kSmall; ++i)
		bytes[i] = static_cast<uint8_t>(i);

	// Far bigger, so the heap very likely moves the block rather than growing it in place
	auto* grown = static_cast<uint8_t*>(Memory::Reallocate(bytes, 1024 * 1024));
	CDL_CHECK(grown != nullptr);
	for (size_t i = 0; i < kSmall; ++i)
		CDL_EXPECT_EQ(grown[i], static_cast<uint8_t>(i));

	Memory::Free(grown);
}

CDL_TEST_CASE(GlobalAllocation, ReallocateOfNullAllocates, Unit)
{
	void* ptr = Memory::Reallocate(nullptr, 64);
	CDL_CHECK(ptr != nullptr);
	std::memset(ptr, 0, 64);
	Memory::Free(ptr);
}

CDL_TEST_CASE(GlobalAllocation, ReallocateToZeroFreesAndReturnsNull, Unit)
{
	void* ptr = Memory::Allocate(64);
	CDL_CHECK(ptr != nullptr);
	CDL_EXPECT(Memory::Reallocate(ptr, 0) == nullptr);
}

// Dirties a block of the same size first, so the heap is likely to hand back memory that isn't already zero
CDL_TEST_CASE(GlobalAllocation, AllocateZeroedZeroesEveryByte, Unit)
{
	constexpr size_t kCount = 256;
	void* dirty = Memory::Allocate(kCount * sizeof(uint32_t));
	CDL_CHECK(dirty != nullptr);
	std::memset(dirty, 0xFF, kCount * sizeof(uint32_t));
	Memory::Free(dirty);

	auto* values = static_cast<uint32_t*>(Memory::AllocateZeroed(kCount, sizeof(uint32_t)));
	CDL_CHECK(values != nullptr);
	for (size_t i = 0; i < kCount; ++i)
		CDL_EXPECT_EQ(values[i], 0u);

	Memory::Free(values);
}

// delete on a null pointer and SDL_free(nullptr) both land here
CDL_TEST_CASE(GlobalAllocation, FreeingNullDoesNothing, Unit)
{
	Memory::Free(nullptr);
	Memory::FreeAligned(nullptr);
}

// In Profile builds new and delete are Candle's, and the aligned forms have to keep the alignment the CRT's would
CDL_TEST_CASE(GlobalAllocation, NewHonoursAnOveralignedType, Unit)
{
	Overaligned* value = new Overaligned{};
	CDL_CHECK(value != nullptr);
	CDL_EXPECT(IsAligned(value, alignof(Overaligned)));
	delete value;

	Overaligned* values = new Overaligned[4]{};
	CDL_CHECK(values != nullptr);
	CDL_EXPECT(IsAligned(values, alignof(Overaligned)));
	delete[] values;
}
