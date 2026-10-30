// MakeImageBarrier turns two ImageStates and a subresource range into one barrier, without recording it.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "RenderTestHelpers.h"

#include <Candle/Render/Vulkan/Image.h>
#include <Candle/Render/Vulkan/RenderDevice.h>

#include <cstdint>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Gpu;
using Candle::Test::Type::Unit;

namespace {

	constexpr ImageState kTransferDst{
		.Layout = vk::ImageLayout::eTransferDstOptimal,
		.Access = vk::AccessFlagBits2::eTransferWrite,
		.Stage = vk::PipelineStageFlagBits2::eClear };

	constexpr ImageState kSampled{
		.Layout = vk::ImageLayout::eShaderReadOnlyOptimal,
		.Access = vk::AccessFlagBits2::eShaderSampledRead,
		.Stage = vk::PipelineStageFlagBits2::eFragmentShader };

}

// Swapping the two sides is the classic slip: it still compiles, and a layout-only check can miss it.
CDL_TEST_CASE(Image, BarrierRunsFromTheFirstStateToTheSecond, Unit)
{
	const vk::Image image{ reinterpret_cast<VkImage>(std::uintptr_t{ 0x1000 }) };	// Never handed to Vulkan
	const vk::ImageSubresourceRange range{ .aspectMask = vk::ImageAspectFlagBits::eColor, .baseMipLevel = 1, .levelCount = 3, .baseArrayLayer = 2, .layerCount = 4 };

	const vk::ImageMemoryBarrier2 barrier = MakeImageBarrier(image, kTransferDst, kSampled, range);

	CDL_EXPECT_EQ(barrier.oldLayout, kTransferDst.Layout);
	CDL_EXPECT(barrier.srcAccessMask == kTransferDst.Access);
	CDL_EXPECT(barrier.srcStageMask == kTransferDst.Stage);
	CDL_EXPECT_EQ(barrier.newLayout, kSampled.Layout);
	CDL_EXPECT(barrier.dstAccessMask == kSampled.Access);
	CDL_EXPECT(barrier.dstStageMask == kSampled.Stage);
	CDL_EXPECT(barrier.image == image);
	CDL_EXPECT(barrier.subresourceRange == range);

	// No queue family ownership transfer: that needs both families named, and a matching barrier on the other queue
	CDL_EXPECT_EQ(barrier.srcQueueFamilyIndex, VK_QUEUE_FAMILY_IGNORED);
	CDL_EXPECT_EQ(barrier.dstQueueFamilyIndex, VK_QUEUE_FAMILY_IGNORED);
}

// An upload's shape over a mip chain: from a fresh ImageState to TransferDst, written, then on to sampled. The clear
// checks every mip reached TransferDst, and the second barrier's oldLayout every mip that stayed there, so a barrier
// covering less than its range fails validation. Starting from ImageState{} also checks the default is a fresh image.
CDL_TEST_CASE(Image, BarrierCoversEveryMipInItsRange, Gpu)
{
	constexpr vk::Format kFormat = vk::Format::eR8G8B8A8Unorm;
	constexpr uint32_t kMipLevels = 3;
	constexpr vk::ImageSubresourceRange kAllMips{ .aspectMask = vk::ImageAspectFlagBits::eColor, .levelCount = kMipLevels, .layerCount = 1 };

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		OwnedImage owned = CreateOwnedImage(gpu.Device, kFormat, { .width = 8, .height = 8 }, kMipLevels,
			vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled);
		TestCommands commands(gpu.Device);

		const vk::ImageMemoryBarrier2 toTransferDst = MakeImageBarrier(*owned.Handle, ImageState{}, kTransferDst, kAllMips);
		const vk::ImageMemoryBarrier2 toSampled = MakeImageBarrier(*owned.Handle, kTransferDst, kSampled, kAllMips);

		commands.Buffer.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
		commands.Buffer.pipelineBarrier2({ .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &toTransferDst });
		commands.Buffer.clearColorImage(*owned.Handle, vk::ImageLayout::eTransferDstOptimal, vk::ClearColorValue(1.0f, 0.0f, 1.0f, 1.0f), kAllMips);
		commands.Buffer.pipelineBarrier2({ .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &toSampled });
		commands.Buffer.end();

		SubmitAndWait(gpu.Device, commands.Buffer);
	}
	ExpectNoVulkanDebugMessages(logs);
}
