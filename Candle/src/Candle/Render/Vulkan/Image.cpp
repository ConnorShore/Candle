#include "cdlpch.h"
#include "Image.h"

#include <vulkan/vulkan_format_traits.hpp>

namespace Candle {

	vk::ImageMemoryBarrier2 MakeImageBarrier(vk::Image image, const ImageState& from, const ImageState& to, const vk::ImageSubresourceRange& range)
	{
		return {
			.srcStageMask = from.Stage,
			.srcAccessMask = from.Access,
			.dstStageMask = to.Stage,
			.dstAccessMask = to.Access,
			.oldLayout = from.Layout,
			.newLayout = to.Layout,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = image,
			.subresourceRange = range
		};
	}

	vk::ImageAspectFlags GetAspectMask(vk::Format format)
	{
		vk::ImageAspectFlags aspect{};
		if (vk::hasDepthComponent(format))
			aspect |= vk::ImageAspectFlagBits::eDepth;
		if (vk::hasStencilComponent(format))
			aspect |= vk::ImageAspectFlagBits::eStencil;

		return aspect ? aspect : vk::ImageAspectFlagBits::eColor;
	}

}
