// GPUAllocator over a real device. Each test fails on any validation message.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "RenderTestHelpers.h"

#include <Candle/Render/Vulkan/GPUAllocator.h>
#include <Candle/Render/Vulkan/GPUBuffer.h>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Gpu;

namespace {

	uint64_t TotalAllocationBytes(const GPUAllocatorStats& stats)
	{
		uint64_t total = 0;
		for (uint32_t i = 0; i < stats.HeapCount; ++i)
			total += stats.Heaps[i].AllocationBytes;
		return total;
	}

}

CDL_TEST_CASE(GPUAllocator, CreatesAndDestroysWithoutValidationMessages, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestAllocator gpu;
		CDL_EXPECT(gpu.Allocator.GetAllocator() != nullptr);
		CDL_EXPECT(gpu.Allocator.GetDevice() == &gpu.Device);
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 0u);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The allocator is created with VMA's device-address flag, which a buffer's address depends on.
CDL_TEST_CASE(GPUAllocator, CreatesBuffersWithDeviceAddresses, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestAllocator gpu;
		GPUBuffer buffer = gpu.Allocator.CreateBuffer({
			.Size = 64,
			.Usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress });

		CDL_EXPECT(buffer.GetBuffer());
		CDL_EXPECT_NE(buffer.GetDeviceAddress(), 0u);
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 1u);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The panel stacks these into one bar, so an allocation must show up in the heap stats and leave again when it's freed.
CDL_TEST_CASE(GPUAllocator, CaptureStatsFollowsAnAllocation, Gpu)
{
	constexpr uint64_t kBufferBytes = 1024 * 1024;

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestAllocator gpu;
		const GPUAllocatorStats before = gpu.Allocator.CaptureStats();
		CDL_CHECK(before.HeapCount > 0u);
		{
			GPUBuffer buffer = gpu.Allocator.CreateBuffer({ .Size = kBufferBytes, .Usage = vk::BufferUsageFlagBits::eStorageBuffer });

			const GPUAllocatorStats during = gpu.Allocator.CaptureStats();
			CDL_EXPECT(TotalAllocationBytes(during) >= TotalAllocationBytes(before) + kBufferBytes);
			for (uint32_t i = 0; i < during.HeapCount; ++i)
			{
				CDL_EXPECT(during.Heaps[i].BlockBytes >= during.Heaps[i].AllocationBytes);
				CDL_EXPECT(during.Heaps[i].BudgetBytes <= during.Heaps[i].SizeBytes);
			}
		}
		CDL_EXPECT_EQ(TotalAllocationBytes(gpu.Allocator.CaptureStats()), TotalAllocationBytes(before));
	}
	ExpectNoVulkanDebugMessages(logs);
}
