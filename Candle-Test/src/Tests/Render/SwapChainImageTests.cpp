// SwapChainImage tracks one VkImage's layout across the barriers it records. Validation checks every barrier's oldLayout
// against the layout the image is really in, so a tracking mistake fails these as a validation message. An oldLayout
// of Undefined is always legal, so each test first moves the image out of it to give a mistake something to contradict.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "RenderTestHelpers.h"

#include <Candle/Render/SwapChainImage.h>
#include <Candle/Render/RenderDevice.h>

#include <type_traits>
#include <utility>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Gpu;

// A copy would be a second owner destroying the same VkImageView.
CDL_STATIC_ASSERT(!std::is_copy_constructible_v<SwapChainImage>);
CDL_STATIC_ASSERT(!std::is_copy_assignable_v<SwapChainImage>);
CDL_STATIC_ASSERT(std::is_move_constructible_v<SwapChainImage>);
CDL_STATIC_ASSERT(std::is_move_assignable_v<SwapChainImage>);

namespace {

	constexpr vk::Format kFormat = vk::Format::eR8G8B8A8Unorm;
	constexpr vk::Extent2D kExtent{ .width = 4, .height = 4 };

	constexpr ImageState kTransferDst{
		.Layout = vk::ImageLayout::eTransferDstOptimal,
		.Access = vk::AccessFlagBits2::eTransferWrite,
		.Stage = vk::PipelineStageFlagBits2::eClear };

	constexpr ImageState kColorAttachment{
		.Layout = vk::ImageLayout::eColorAttachmentOptimal,
		.Access = vk::AccessFlagBits2::eColorAttachmentWrite,
		.Stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput };

	// Shaped like a swapchain image: one colour mip, written as an attachment, plus TransferDst for the test's clear
	OwnedImage CreateSwapChainLikeImage(RenderDevice& device)
	{
		return CreateOwnedImage(device, kFormat, kExtent, 1, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst);
	}

	SwapChainImage WrapImage(RenderDevice& device, const OwnedImage& owned)
	{
		vk::raii::ImageView view(device.GetDevice(), vk::ImageViewCreateInfo{
			.image = *owned.Handle,
			.viewType = vk::ImageViewType::e2D,
			.format = kFormat,
			.subresourceRange = SwapChainImage::kSubresourceRange });
		return SwapChainImage(*owned.Handle, std::move(view), ImageDesc{ .Extent = kExtent, .Format = kFormat });
	}

}

CDL_TEST_CASE(SwapChainImage, KeepsItsDescriptionAndView, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		OwnedImage owned = CreateSwapChainLikeImage(gpu.Device);
		SwapChainImage image = WrapImage(gpu.Device, owned);

		CDL_EXPECT_EQ(image.GetDesc().Extent.width, kExtent.width);
		CDL_EXPECT_EQ(image.GetDesc().Extent.height, kExtent.height);
		CDL_EXPECT_EQ(image.GetDesc().Format, kFormat);
		CDL_EXPECT(image.GetImageView());
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The clear checks the first barrier's newLayout, and the second barrier's oldLayout must be that same layout.
CDL_TEST_CASE(SwapChainImage, TransitionsFromTheLayoutItLastMovedTo, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		OwnedImage owned = CreateSwapChainLikeImage(gpu.Device);
		SwapChainImage image = WrapImage(gpu.Device, owned);
		TestCommands commands(gpu.Device);

		commands.Buffer.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
		image.Transition(kTransferDst, commands.Buffer);
		commands.Buffer.clearColorImage(*owned.Handle, vk::ImageLayout::eTransferDstOptimal, vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f), SwapChainImage::kSubresourceRange);
		image.Transition(kColorAttachment, commands.Buffer);
		commands.Buffer.end();

		SubmitAndWait(gpu.Device, commands.Buffer);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// A layout change the SwapChainImage never recorded, as presentation makes, must be adopted through SetState. Without it
// the next Transition names TransferDst as its oldLayout while the image is really in General.
CDL_TEST_CASE(SwapChainImage, SetStateAdoptsALayoutChangedElsewhere, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		OwnedImage owned = CreateSwapChainLikeImage(gpu.Device);
		SwapChainImage image = WrapImage(gpu.Device, owned);
		TestCommands commands(gpu.Device);

		commands.Buffer.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
		image.Transition(kTransferDst, commands.Buffer);

		const ImageState general{ .Layout = vk::ImageLayout::eGeneral, .Access = vk::AccessFlagBits2::eNone, .Stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput };
		const vk::ImageMemoryBarrier2 elsewhere = MakeImageBarrier(*owned.Handle, kTransferDst, general, SwapChainImage::kSubresourceRange);
		commands.Buffer.pipelineBarrier2({ .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &elsewhere });

		image.SetState(general);
		image.Transition(kColorAttachment, commands.Buffer);
		commands.Buffer.end();

		SubmitAndWait(gpu.Device, commands.Buffer);
	}
	ExpectNoVulkanDebugMessages(logs);
}
