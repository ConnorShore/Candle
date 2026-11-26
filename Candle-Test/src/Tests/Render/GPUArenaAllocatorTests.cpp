// GPUArenaAllocator on a real device: a bump allocator over one mapped buffer, which a frame slot rewinds once its frame
// retires. Each GPU test fails on any validation message.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "RenderTestHelpers.h"

#include <Candle/Render/Resources/GPUArenaAllocator.h>
#include <Candle/Render/Vulkan/GPUAllocator.h>

#include <cstdint>
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
