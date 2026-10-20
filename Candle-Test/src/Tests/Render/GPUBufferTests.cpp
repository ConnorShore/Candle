// GPUBuffer through VMA on a real device: creation, its device address, host writes, and single ownership across
// moves, checked against VMA's live allocation count. Each GPU test fails on any validation message.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "RenderTestHelpers.h"

#include <Candle/Render/GPUAllocator.h>
#include <Candle/Render/GPUBuffer.h>

#include <span>
#include <type_traits>
#include <utility>
#include <vector>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Gpu;
using Candle::Test::Type::Unit;

// A copy would be a second owner freeing the same allocation. Default-constructible for slots filled later.
CDL_STATIC_ASSERT(std::is_default_constructible_v<GPUBuffer>);
CDL_STATIC_ASSERT(!std::is_copy_constructible_v<GPUBuffer>);
CDL_STATIC_ASSERT(!std::is_copy_assignable_v<GPUBuffer>);
CDL_STATIC_ASSERT(std::is_nothrow_move_constructible_v<GPUBuffer>);
CDL_STATIC_ASSERT(std::is_nothrow_move_assignable_v<GPUBuffer>);

namespace {

	constexpr vk::DeviceSize kSize = 256;

	GPUBuffer CreateStorageBuffer(GPUAllocator& allocator)
	{
		return allocator.CreateBuffer({ .m_Size = kSize, .m_Usage = vk::BufferUsageFlagBits::eStorageBuffer });
	}

	void ExpectEmpty(const GPUBuffer& buffer)
	{
		CDL_EXPECT(!buffer.GetBuffer());
		CDL_EXPECT(buffer.GetAllocation() == nullptr);
		CDL_EXPECT_EQ(buffer.GetSize(), 0u);
		CDL_EXPECT_EQ(buffer.GetDeviceAddress(), 0u);
	}

}

// Destroying it must not reach vmaDestroyBuffer, which asserts on the null allocator before checking the handles.
CDL_TEST_CASE(GPUBuffer, DefaultIsEmptyAndDestroysSafely, Unit)
{
	GPUBuffer buffer;
	ExpectEmpty(buffer);
}

CDL_TEST_CASE(GPUBuffer, CreatesWithTheRequestedSizeAndFreesOnDestruction, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestAllocator gpu;
		{
			GPUBuffer buffer = CreateStorageBuffer(gpu.Allocator);
			CDL_EXPECT(buffer.GetBuffer());
			CDL_EXPECT(buffer.GetAllocation() != nullptr);
			CDL_EXPECT_EQ(buffer.GetSize(), kSize);
			CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 1u);
		}
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 0u);
	}
	ExpectNoVulkanDebugMessages(logs);
}

CDL_TEST_CASE(GPUBuffer, HasADeviceAddressOnlyWhenCreatedForOne, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestAllocator gpu;
		const GPUBufferSpec addressable{ .m_Size = kSize, .m_Usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress };
		GPUBuffer first = gpu.Allocator.CreateBuffer(addressable);
		GPUBuffer second = gpu.Allocator.CreateBuffer(addressable);
		GPUBuffer plain = CreateStorageBuffer(gpu.Allocator);

		CDL_EXPECT_NE(first.GetDeviceAddress(), 0u);
		CDL_EXPECT_NE(second.GetDeviceAddress(), 0u);
		CDL_EXPECT_NE(first.GetDeviceAddress(), second.GetDeviceAddress());
		CDL_EXPECT_EQ(plain.GetDeviceAddress(), 0u);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Two writes at different offsets, so a write that ignored its offset would overwrite the first half.
CDL_TEST_CASE(GPUBuffer, WriteCopiesIntoTheAllocationAtItsOffset, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestAllocator gpu;
		GPUBuffer buffer = CreateHostBuffer(gpu.Allocator, kSize);
		const std::vector<std::byte> expected = BytePattern(kSize, 7);
		const std::span<const std::byte> bytes(expected);

		buffer.Write(bytes.first(kSize / 2));
		buffer.Write(bytes.subspan(kSize / 2), kSize / 2);

		CDL_EXPECT(ReadBack(gpu.Allocator, buffer) == expected);
	}
	ExpectNoVulkanDebugMessages(logs);
}

CDL_TEST_CASE(GPUBuffer, MoveConstructTransfersOwnership, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestAllocator gpu;
		{
			GPUBuffer original = gpu.Allocator.CreateBuffer({
				.m_Size = kSize,
				.m_Usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress });
			const vk::Buffer handle = original.GetBuffer();
			const vk::DeviceAddress address = original.GetDeviceAddress();

			GPUBuffer moved(std::move(original));

			ExpectEmpty(original);
			CDL_EXPECT(moved.GetBuffer() == handle);
			CDL_EXPECT_EQ(moved.GetSize(), kSize);
			CDL_EXPECT_EQ(moved.GetDeviceAddress(), address);
			CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 1u);
		}
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 0u);
	}
	ExpectNoVulkanDebugMessages(logs);
}

CDL_TEST_CASE(GPUBuffer, MoveAssignFreesTheBufferItOverwrites, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestAllocator gpu;
		{
			GPUBuffer target = CreateStorageBuffer(gpu.Allocator);
			GPUBuffer source = CreateStorageBuffer(gpu.Allocator);
			const vk::Buffer sourceHandle = source.GetBuffer();
			CDL_CHECK_EQ(LiveAllocations(gpu.Allocator), 2u);

			target = std::move(source);

			ExpectEmpty(source);
			CDL_EXPECT(target.GetBuffer() == sourceHandle);
			CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 1u);
		}
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 0u);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// FrameContext's pattern: a default slot assigned its buffer after construction.
CDL_TEST_CASE(GPUBuffer, MoveAssignIntoAnEmptySlotTakesOwnership, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestAllocator gpu;
		{
			GPUBuffer slot;
			slot = CreateStorageBuffer(gpu.Allocator);

			CDL_EXPECT(slot.GetBuffer());
			CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 1u);
		}
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 0u);
	}
	ExpectNoVulkanDebugMessages(logs);
}

CDL_TEST_CASE(GPUBuffer, SelfMoveAssignKeepsTheBuffer, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestAllocator gpu;
		GPUBuffer buffer = CreateStorageBuffer(gpu.Allocator);
		const vk::Buffer handle = buffer.GetBuffer();
		GPUBuffer& alias = buffer;	// Through a reference, so the compiler doesn't flag the self-move

		buffer = std::move(alias);

		CDL_EXPECT(buffer.GetBuffer() == handle);
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 1u);
	}
	ExpectNoVulkanDebugMessages(logs);
}
