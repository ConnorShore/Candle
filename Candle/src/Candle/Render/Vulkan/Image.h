#pragma once

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>

#include <cstdint>

namespace Candle {

	struct ImageDesc
	{
		glm::uvec2 Extent{ 0, 0 };
		vk::Format Format{ vk::Format::eUndefined };
		vk::ImageUsageFlags Usage{};
		uint8_t MipLevels{ 1 };
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

	// Every aspect the format has. Without separateDepthStencilLayouts a barrier on a depth-stencil image must name both.
	vk::ImageAspectFlags GetAspectMask(vk::Format format);

}
