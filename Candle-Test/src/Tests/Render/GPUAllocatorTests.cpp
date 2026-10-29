// GPUAllocator over a real device. Each test fails on any validation message.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "RenderTestHelpers.h"

#include <Candle/Render/GPUAllocator.h>
#include <Candle/Render/GPUBuffer.h>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Gpu;

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
