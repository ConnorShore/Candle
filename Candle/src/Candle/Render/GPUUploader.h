#pragma once

#include "DeletionQueue.h"

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

	struct ImageUpload
	{
		std::span<const std::byte> Data;
		vk::Image Destination;
		vk::ImageLayout DestinationLayout;
	};

	// Records and submits one-off copies on the graphics queue.
	// Threading: main thread only; it owns its command pool.
	class GPUUploader
	{
	public:
		GPUUploader(RenderDevice& renderDevice, GPUAllocator& allocator);
		~GPUUploader();	// Waits for the last upload, so its staging buffer is never freed in use

		// Delete copy and move assignment
		GPUUploader(const GPUUploader&) = delete;
		GPUUploader(GPUUploader&&) = delete;
		GPUUploader& operator=(const GPUUploader&) = delete;
		GPUUploader& operator=(GPUUploader&&) = delete;

		// Stages every region, records the copies plus one barrier into dstStages/dstAccess, and submits without waiting.
		void UploadBuffers(std::span<const BufferUpload> uploads, vk::PipelineStageFlags2 dstStages, vk::AccessFlags2 dstAccess);
		void UploadImage(const ImageUpload upload, vk::PipelineStageFlags2 dstStages, vk::AccessFlags2 dstAccess);

		void FreeCompletedStagingBuffers();	// Frees the staging buffers of uploads the GPU has finished

	private:
		RenderDevice& m_RenderDevice;
		GPUAllocator& m_Allocator;
		vk::raii::CommandPool m_CommandPool{ nullptr };
		vk::raii::CommandBuffer m_CommandBuffer{ nullptr };

		DeletionQueue m_StagingDeletions;
		uint64_t m_LastUploadValue{ 0 };
	};

}