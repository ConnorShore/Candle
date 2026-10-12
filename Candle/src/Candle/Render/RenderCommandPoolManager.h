#pragma once

#include "RenderConstants.h"

#include <array>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

namespace Candle {

	// TODO: Probably only want to pass vk::raii::Device to these classes to keep it more lightweight
	// but we may need additional fucntionality from this so keeping it just in case for now
	class RenderDevice;

	// Manages command pools and buffers for a single queue family and frame
	// Threading: render thread only; command buffers are aquired and recorded on worker threads, but the pool is created and destroyed on the render thread
	
	class ThreadFrameCommands
	{
	public:
		ThreadFrameCommands(RenderDevice& renderDevice, uint32_t queueFamilyIndex);

		vk::raii::CommandBuffer& AquireCommandBuffer(); // Returns a command buffer for recording (usually for a thread to record commands for a frame)
		void Reset(); // Resets the command pool and all command buffers; called at the start of a frame

	private:
		RenderDevice& m_RenderDevice;
		vk::raii::CommandPool m_CommandPool{ nullptr };
		std::vector<vk::raii::CommandBuffer> m_PrimaryCommandBuffers; //	No need to synchronize access to vector as each thread has its own instance

		uint32_t m_PrimaryCommandBuffersUsed{ 0 };	// Number of primary command buffers used in the current frame; if we run out, we create a new one
	};

	// Manages command pools and buffers for all queue families, threads, frames, etc
	// Threading: render thread only; command buffers are recorded on worker threads, but the pools are created and destroyed on the render thread
	class RenderCommandPoolManager
	{
	public:
		RenderCommandPoolManager(RenderDevice& renderDevice, uint32_t threadCount);

		void BeginFrame(uint32_t frameIndex);

		inline ThreadFrameCommands& GetGraphicsCommands(uint32_t frameIndex, uint32_t threadIndex) { return m_GraphicsCommands[frameIndex][threadIndex]; }
		//inline ThreadFrameCommands& GetComputeCommands(uint32_t frameIndex, uint32_t threadIndex) { return m_ComputeCommands[frameIndex][threadIndex]; }

	private:
		// [queue family index][frame index][thread index]
		std::array<std::vector<ThreadFrameCommands>, kMaxFramesInFlight> m_GraphicsCommands;
		//std::array<std::vector<ThreadFrameCommands>, kMaxFramesInFlight>  m_ComputeCommands;
	};

}