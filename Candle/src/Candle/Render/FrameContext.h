#pragma once

#include "Image.h"
#include "GPUBuffer.h"

#include <vulkan/vulkan_raii.hpp>

namespace Candle {

	// One per frame in flight. Threading: render thread only.
	struct FrameContext
	{
		vk::raii::CommandPool CommandPool{ nullptr };		// The render thread's own; workers record from RenderCommandPoolManager
		vk::raii::CommandBuffer CommandBuffer{ nullptr };
		GPUBuffer FrameData;				// This slot's GPU::FrameData. Host-written only once RetireValue is reached; shaders read it by address
		uint64_t RetireValue{ 0 };			// Used for the timeline semaphore to know when the GPU has finished with this frame context and it can be reused
	};
}