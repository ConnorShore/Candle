// GPUArenaAllocator on a real device: a bump allocator over one mapped buffer, which a frame slot rewinds once its frame
// retires. Each GPU test fails on any validation message.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "RenderTestHelpers.h"

#include <Candle/Render/Resources/GPUArenaAllocator.h>
#include <Candle/Render/Vulkan/GPUAllocator.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <span>
#include <type_traits>
#include <utility>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Gpu;

// A copy would be a second owner freeing the same buffer. Default-constructible for the FrameContext slot filled later.
CDL_STATIC_ASSERT(std::is_default_constructible_v<GPUArenaAllocator>);
CDL_STATIC_ASSERT(!std::is_copy_constructible_v<GPUArenaAllocator>);
CDL_STATIC_ASSERT(!std::is_copy_assignable_v<GPUArenaAllocator>);
CDL_STATIC_ASSERT(std::is_nothrow_move_constructible_v<GPUArenaAllocator>);
CDL_STATIC_ASSERT(std::is_nothrow_move_assignable_v<GPUArenaAllocator>);

namespace {

	constexpr uint32_t kCapacity = 4 * 1024;

}

// Padding to an allocation's alignment is used up too. The peak outlasts Reset, so it's the most one frame has used.
CDL_TEST_CASE(GPUArenaAllocator, StatsCountPaddingAndKeepThePeakAcrossResets, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestAllocator gpu;
		GPUArenaAllocator arena(gpu.Allocator, kCapacity, "Test Arena");
		CDL_EXPECT_EQ(arena.GetStats().CapacityBytes, uint64_t{ kCapacity });

		arena.Allocate(100, 16);
		arena.Allocate(16, 16);
		CDL_EXPECT_EQ(arena.GetStats().UsedBytes, 128u);
		CDL_EXPECT_EQ(arena.GetStats().PeakBytes, 128u);

		arena.Reset();
		arena.Allocate(64, 16);
		CDL_EXPECT_EQ(arena.GetStats().UsedBytes, 64u);
		CDL_EXPECT_EQ(arena.GetStats().PeakBytes, 128u);

		arena.Allocate(256, 16);
		CDL_EXPECT_EQ(arena.GetStats().PeakBytes, 320u);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// FrameContext's pattern: an empty slot assigned its arena after construction. The buffer moves with it, so what was handed
// out before still points into the buffer the arena keeps allocating from.
CDL_TEST_CASE(GPUArenaAllocator, MoveKeepsItsBufferAndWhatItHandedOut, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestAllocator gpu;
		GPUArenaAllocator original(gpu.Allocator, kCapacity, "Test Arena");
		const GPUArenaAllocation first = original.Allocate(64, 16);

		GPUArenaAllocator slot;
		slot = std::move(original);
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 1u);
		CDL_EXPECT_EQ(slot.GetStats().UsedBytes, 64u);

		const GPUArenaAllocation second = slot.Allocate(64, 16);
		CDL_EXPECT(second.Buffer == first.Buffer);
		CDL_EXPECT_EQ(second.Address, first.Address + 64);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Each push lands in its own bytes of the buffer the GPU reads. Write-combined memory is slow to read back, but that's
// fine for a few bytes.
CDL_TEST_CASE(GPUArenaAllocator, PushesKeepTheirOwnBytesInMappedMemory, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestAllocator gpu;
		GPUArenaAllocator arena(gpu.Allocator, kCapacity, "Test Arena");

		const std::array<uint32_t, 3> first{ 0x11111111, 0x22222222, 0x33333333 };
		const std::array<uint32_t, 5> second{ 0xAAAAAAAA, 0xBBBBBBBB, 0xCCCCCCCC, 0xDDDDDDDD, 0xEEEEEEEE };
		const GPUArenaAllocation firstAllocation = arena.Push(std::as_bytes(std::span<const uint32_t>(first)));
		const GPUArenaAllocation secondAllocation = arena.Push(std::as_bytes(std::span<const uint32_t>(second)));

		CDL_CHECK_EQ(firstAllocation.Mapped.size(), sizeof(first));
		CDL_CHECK_EQ(secondAllocation.Mapped.size(), sizeof(second));
		CDL_EXPECT(std::memcmp(firstAllocation.Mapped.data(), first.data(), sizeof(first)) == 0);
		CDL_EXPECT(std::memcmp(secondAllocation.Mapped.data(), second.data(), sizeof(second)) == 0);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Shaders load through the address, so it's the address that has to be aligned, not just the offset. A push of bytes
// asks for an alignment of 1 and still gets 16, and a larger alignment is kept.
CDL_TEST_CASE(GPUArenaAllocator, AllocationsAreAlignedToAtLeast16Bytes, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestAllocator gpu;
		GPUArenaAllocator arena(gpu.Allocator, kCapacity, "Test Arena");

		const GPUArenaAllocation first = arena.Allocate(1, 1);
		const GPUArenaAllocation second = arena.Allocate(1, 1);
		const GPUArenaAllocation overAligned = arena.Allocate(1, 256);

		CDL_EXPECT_EQ(second.Offset, 16u);
		CDL_EXPECT_EQ(overAligned.Offset, 256u);
		CDL_EXPECT_EQ(first.Address % 16, 0u);
		CDL_EXPECT_EQ(second.Address % 16, 0u);
		CDL_EXPECT_EQ(overAligned.Address % 16, 0u);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// A frame slot gets the same memory back each time its frame comes round: Reset rewinds, it doesn't free or allocate
CDL_TEST_CASE(GPUArenaAllocator, ResetHandsOutTheSameMemoryAgain, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestAllocator gpu;
		GPUArenaAllocator arena(gpu.Allocator, kCapacity, "Test Arena");

		const GPUArenaAllocation before = arena.Allocate(64, 16);
		arena.Allocate(128, 16);
		arena.Reset();
		const GPUArenaAllocation after = arena.Allocate(64, 16);

		CDL_EXPECT_EQ(after.Offset, 0u);
		CDL_EXPECT_EQ(after.Address, before.Address);
		CDL_EXPECT(after.Mapped.data() == before.Mapped.data());
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 1u);
	}
	ExpectNoVulkanDebugMessages(logs);
}
