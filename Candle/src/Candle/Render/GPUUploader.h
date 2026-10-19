#pragma once

#include <span>
#include <vulkan/vulkan_raii.hpp>

namespace Candle {

	class RenderDevice;
	class GPUAllocator;

	struct BufferUpload 
	{ 
		std::span<const std::byte> Data; 
		vk::Buffer Destination; 
	};

	// Records and submits one-off copies on the graphics queue.
	// Threading: main thread only; it owns its command pool.
	class GPUUploader
	{
	public:
		GPUUploader(RenderDevice& renderDevice, GPUAllocator& allocator);
		~GPUUploader() = default;

		// Delete copy and move assignment
		GPUUploader(const GPUUploader&) = delete;
		GPUUploader(GPUUploader&&) = delete;
		GPUUploader& operator=(const GPUUploader&) = delete;
		GPUUploader& operator=(GPUUploader&&) = delete;

		// Stages every region, records the copies plus one barrier into dstStages/dstAccess, and submits.
		// Blocks until the GPU has finished, so staging is freed on return; the deletion queue removes the wait later.
		void UploadBuffers(std::span<const BufferUpload> uploads, vk::PipelineStageFlags2 dstStages, vk::AccessFlags2 dstAccess);

	private:
		RenderDevice& m_RenderDevice;
		GPUAllocator& m_Allocator;
		vk::raii::CommandPool m_CommandPool{ nullptr };
		vk::raii::CommandBuffer m_CommandBuffer{ nullptr };
	};

}