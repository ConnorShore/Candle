#pragma once

#include "Candle/Render/Vulkan/Image.h"
#include "Candle/Render/Vulkan/GPUBuffer.h"

#include <vulkan/vulkan_raii.hpp>

namespace Candle {

	// One per frame in flight. Threading: render thread only.
	struct FrameContext
	{
		vk::raii::CommandPool CommandPool{ nullptr };		// The render thread's own; workers record from RenderCommandPoolManager
		vk::raii::CommandBuffer CommandBuffer{ nullptr };
		// TODO: In future have a GPUArenaAllocator instead of separate buffers for frame data, transform data, etc, 
		//	 so the GPU can allocate from it instead of the CPU writing into a buffer
		GPUBuffer FrameData;				// ost-written only once RetireValue is reached; shaders read it by address
		GPUBuffer TransformData;			// Host-written only once RetireValue is reached; shaders read it by address
		GPUBuffer GuiVertices;				// Host-written only once RetireValue is reached; shaders read it by address
		GPUBuffer GuiIndices;				// Host-written only once RetireValue is reached; shaders read it by address
		uint64_t RetireValue{ 0 };			// Used for the timeline semaphore to know when the GPU has finished with this frame context and it can be reused
	};
}