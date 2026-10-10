#pragma once

#include "Image.h"

#include <vulkan/vulkan_raii.hpp>

namespace Candle {

	struct FrameContext
	{
		vk::raii::CommandPool CommandPool{ nullptr };
		vk::raii::CommandBuffer CommandBuffer{ nullptr };
		uint64_t RetireValue{ 0 };			// Used for the timeline semaphore to know when the GPU has finished with this frame context and it can be reused
	};
}