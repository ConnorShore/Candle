#pragma once

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <limits>
#include <cstdint>

namespace Candle {

	// Values, not references into the pool, so an info stays safe to hold after its texture is released
	struct TextureInfo
	{
		uint32_t Index{ std::numeric_limits<uint32_t>::max() };
		vk::ImageView ImageView{ nullptr };
		vk::Image Image{ nullptr };		// For barriers, when the renderer draws into a render target
		vk::Format Format{ vk::Format::eUndefined };
		glm::uvec2 Extent{ 0, 0 };
		uint32_t SizeBytes{ 0 };
		uint8_t MipLevels{ 1 };
		vk::ImageLayout SampledLayout{ vk::ImageLayout::eShaderReadOnlyOptimal };	// What its bindless descriptor expects, so it must be in this whenever it's sampled
		vk::ImageUsageFlags Usage{};	// eColorAttachment is what lets the scene draw into it
	};

}