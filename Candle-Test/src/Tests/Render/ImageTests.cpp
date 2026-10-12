// Image tracks one VkImage's layout across the barriers it records. Validation checks every barrier's oldLayout
// against the layout the image is really in, so a tracking mistake fails these as a validation message. An oldLayout
// of Undefined is always legal, so each test first moves the image out of it to give a mistake something to contradict.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "RenderTestHelpers.h"

#include <Candle/Render/Image.h>
#include <Candle/Render/RenderDevice.h>

#include <type_traits>
#include <utility>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Gpu;

// A copy would be a second owner destroying the same VkImageView.
CDL_STATIC_ASSERT(!std::is_copy_constructible_v<Image>);
CDL_STATIC_ASSERT(!std::is_copy_assignable_v<Image>);
CDL_STATIC_ASSERT(std::is_move_constructible_v<Image>);
CDL_STATIC_ASSERT(std::is_move_assignable_v<Image>);

namespace {

	constexpr vk::Format kFormat = vk::Format::eR8G8B8A8Unorm;
	constexpr vk::Extent2D kExtent{ .width = 4, .height = 4 };
	constexpr vk::ImageSubresourceRange kColorRange{ .aspectMask = vk::ImageAspectFlagBits::eColor, .levelCount = 1, .layerCount = 1 };

	constexpr ImageTransitionParams kToTransferDst{
		.Layout = vk::ImageLayout::eTransferDstOptimal,
		.AccessMask = vk::AccessFlagBits2::eTransferWrite,
		.StageMask = vk::PipelineStageFlagBits2::eClear,
		.AspectMask = vk::ImageAspectFlagBits::eColor };

	constexpr ImageTransitionParams kToColorAttachment{
		.Layout = vk::ImageLayout::eColorAttachmentOptimal,
		.AccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
		.StageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		.AspectMask = vk::ImageAspectFlagBits::eColor };

	// Image wraps a VkImage it doesn't own, as with swapchain images, so the test owns the image and its memory.
	// Memory is declared first so the image is destroyed before the memory it is bound to.
	struct OwnedImage
	{
		vk::raii::DeviceMemory Memory{ nullptr };
		vk::raii::Image Handle{ nullptr };
	};

	uint32_t FindDeviceLocalMemoryType(RenderDevice& device, uint32_t allowedTypes)
	{
		const vk::PhysicalDeviceMemoryProperties properties = device.GetPhysicalDevice().getMemoryProperties();
		for (uint32_t i = 0; i < properties.memoryTypeCount; ++i)
		{
			if ((allowedTypes & (1u << i)) && (properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal))
				return i;
		}

		CDL_CHECK_MSG(false, "no device-local memory type fits the image");
		return 0;
	}

	OwnedImage CreateOwnedImage(RenderDevice& device)
	{
		OwnedImage image;
		image.Handle = vk::raii::Image(device.GetDevice(), vk::ImageCreateInfo{
			.imageType = vk::ImageType::e2D,
			.format = kFormat,
			.extent = { kExtent.width, kExtent.height, 1 },
			.mipLevels = 1,
			.arrayLayers = 1,
			.samples = vk::SampleCountFlagBits::e1,
			.tiling = vk::ImageTiling::eOptimal,
			.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst,
			.initialLayout = vk::ImageLayout::eUndefined });

		const vk::MemoryRequirements requirements = image.Handle.getMemoryRequirements();
		image.Memory = vk::raii::DeviceMemory(device.GetDevice(), vk::MemoryAllocateInfo{
			.allocationSize = requirements.size,
			.memoryTypeIndex = FindDeviceLocalMemoryType(device, requirements.memoryTypeBits) });
		image.Handle.bindMemory(*image.Memory, 0);
		return image;
	}

	Image WrapImage(RenderDevice& device, const OwnedImage& owned)
	{
		vk::raii::ImageView view(device.GetDevice(), vk::ImageViewCreateInfo{
			.image = *owned.Handle,
			.viewType = vk::ImageViewType::e2D,
			.format = kFormat,
			.subresourceRange = kColorRange });
		return Image(*owned.Handle, std::move(view), ImageDesc{ .Extent = kExtent, .Format = kFormat });
	}

}

CDL_TEST_CASE(Image, KeepsItsDescriptionAndView, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		OwnedImage owned = CreateOwnedImage(gpu.Device);
		Image image = WrapImage(gpu.Device, owned);

		CDL_EXPECT_EQ(image.GetDesc().Extent.width, kExtent.width);
		CDL_EXPECT_EQ(image.GetDesc().Extent.height, kExtent.height);
		CDL_EXPECT_EQ(image.GetDesc().Format, kFormat);
		CDL_EXPECT(*image.GetImageView());
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The clear checks the first barrier's newLayout, and the second barrier's oldLayout must be that same layout.
CDL_TEST_CASE(Image, TransitionsFromTheLayoutItLastMovedTo, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		OwnedImage owned = CreateOwnedImage(gpu.Device);
		Image image = WrapImage(gpu.Device, owned);
		TestCommands commands(gpu.Device);

		commands.Buffer.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
		image.Transition(kToTransferDst, commands.Buffer);
		commands.Buffer.clearColorImage(*owned.Handle, vk::ImageLayout::eTransferDstOptimal, vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f), kColorRange);
		image.Transition(kToColorAttachment, commands.Buffer);
		commands.Buffer.end();

		SubmitAndWait(gpu.Device, commands.Buffer);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// A layout change the Image never recorded, as presentation makes, must be adopted through SetState. Without it the
// next Transition names TransferDst as its oldLayout while the image is really in General.
CDL_TEST_CASE(Image, SetStateAdoptsALayoutChangedElsewhere, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		OwnedImage owned = CreateOwnedImage(gpu.Device);
		Image image = WrapImage(gpu.Device, owned);
		TestCommands commands(gpu.Device);

		commands.Buffer.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
		image.Transition(kToTransferDst, commands.Buffer);

		const vk::ImageMemoryBarrier2 elsewhere{
			.srcStageMask = vk::PipelineStageFlagBits2::eClear,
			.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
			.dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			.dstAccessMask = vk::AccessFlagBits2::eNone,
			.oldLayout = vk::ImageLayout::eTransferDstOptimal,
			.newLayout = vk::ImageLayout::eGeneral,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = *owned.Handle,
			.subresourceRange = kColorRange };
		commands.Buffer.pipelineBarrier2({ .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &elsewhere });

		image.SetState(vk::ImageLayout::eGeneral, vk::AccessFlagBits2::eNone, vk::PipelineStageFlagBits2::eColorAttachmentOutput);
		image.Transition(kToColorAttachment, commands.Buffer);
		commands.Buffer.end();

		SubmitAndWait(gpu.Device, commands.Buffer);
	}
	ExpectNoVulkanDebugMessages(logs);
}
