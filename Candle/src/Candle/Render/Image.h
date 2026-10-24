#pragma once

#include <vulkan/vulkan.hpp>

namespace Candle {

	struct ImageDesc
	{
		vk::Extent2D Extent;
		vk::Format	 Format;
	};

	// The state an image was last left in: what a barrier transitions from or to. The default is a fresh image.
	struct ImageState
	{
		vk::ImageLayout         Layout{ vk::ImageLayout::eUndefined };
		vk::AccessFlags2        Access{ vk::AccessFlagBits2::eNone };
		vk::PipelineStageFlags2 Stage{ vk::PipelineStageFlagBits2::eNone };
	};

	// Builds rather than records, so callers can batch several barriers into one DependencyInfo
	vk::ImageMemoryBarrier2 MakeImageBarrier(vk::Image image, const ImageState& from, const ImageState& to, const vk::ImageSubresourceRange& range);

}
