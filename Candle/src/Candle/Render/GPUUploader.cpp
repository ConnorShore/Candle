#include "cdlpch.h"
#include "GPUUploader.h"
#include "RenderDevice.h"
#include "GPUAllocator.h"
#include "Image.h"

#include "Candle/Platform/Platform.h"

namespace Candle {

	GPUUploader::GPUUploader(RenderDevice& renderDevice, GPUAllocator& allocator)
		: m_RenderDevice(renderDevice)
		, m_Allocator(allocator)
	{
		vk::raii::Device& device = m_RenderDevice.GetDevice();

		// TODO: This will probably be a transfer in the future
		vk::CommandPoolCreateInfo poolInfo{ .queueFamilyIndex = m_RenderDevice.GetQueueFamily(QueueType::Graphics) };

		m_CommandPool = vk::raii::CommandPool(device, poolInfo);
		m_RenderDevice.SetDebugName(*m_CommandPool, "Uploader Command Pool");

		vk::CommandBufferAllocateInfo allocInfo{
			.commandPool = m_CommandPool,
			.level = vk::CommandBufferLevel::ePrimary,
			.commandBufferCount = 1
		};
		m_CommandBuffer = std::move(vk::raii::CommandBuffers(device, allocInfo).front());
		m_RenderDevice.SetDebugName(*m_CommandBuffer, "Uploader Command Buffer");
	}

	GPUUploader::~GPUUploader()
	{
		m_RenderDevice.WaitFor(QueueType::Graphics, m_LastUploadValue);
	}

	void GPUUploader::UploadBuffers(std::span<const BufferUpload> uploads, vk::PipelineStageFlags2 dstStages, vk::AccessFlags2 dstAccess)
	{
		CDL_CORE_ASSERT(Platform::IsMainThread(), "GPUUploader owns its command pool, so it is main thread only");

		// One staging buffer holds everything
		vk::DeviceSize totalSize = 0;
		for (const BufferUpload& upload : uploads)
			totalSize += upload.Data.size_bytes();
		if (totalSize == 0)
			return;

		GPUBuffer stagingBuffer = m_Allocator.CreateBuffer({
			.Size = totalSize,
			.Usage = vk::BufferUsageFlagBits::eTransferSrc,
			.Flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
			.DebugName = "Buffer Staging"
			});

		// The pool can't be reset while the previous upload may still be executing
		m_RenderDevice.WaitFor(QueueType::Graphics, m_LastUploadValue);
		m_StagingDeletions.Drain(QueueType::Graphics, m_LastUploadValue);
		m_CommandPool.reset();
		m_CommandBuffer.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

		vk::DeviceSize stagingOffset = 0;
		for (const BufferUpload& upload : uploads)
		{
			if (upload.Data.empty())
				continue;

			// Maps, copies and unmaps, flushing too since sequential-write memory need not be host-coherent
			[[maybe_unused]] VkResult result = vmaCopyMemoryToAllocation(m_Allocator.GetAllocator(), upload.Data.data(), stagingBuffer.GetAllocation(), stagingOffset, upload.Data.size_bytes());
			CDL_CORE_ASSERT(result == VK_SUCCESS, "Failed to write the staging buffer");

			vk::BufferCopy2 copyRegion{ .srcOffset = stagingOffset, .dstOffset = 0, .size = upload.Data.size_bytes() };
			m_CommandBuffer.copyBuffer2({ .srcBuffer = stagingBuffer.GetBuffer(), .dstBuffer = upload.Destination, .regionCount = 1, .pRegions = &copyRegion });
			stagingOffset += upload.Data.size_bytes();
		}

		// One barrier after every copy; the caller names the stages and accesses that will read the data
		vk::MemoryBarrier2 barrier{
			.srcStageMask = vk::PipelineStageFlagBits2::eCopy,
			.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
			.dstStageMask = dstStages,
			.dstAccessMask = dstAccess };
		m_CommandBuffer.pipelineBarrier2({ .memoryBarrierCount = 1, .pMemoryBarriers = &barrier });
		m_CommandBuffer.end();

		// Graphics queue: frames submitted after this are ordered behind the barrier, so they need no semaphore wait
		vk::CommandBufferSubmitInfo commandBufferInfo{ .commandBuffer = *m_CommandBuffer };
		m_LastUploadValue = m_RenderDevice.Submit(QueueType::Graphics, { .CommandBuffers = { &commandBufferInfo, 1 } });
		m_StagingDeletions.Enqueue(QueueType::Graphics, m_LastUploadValue, std::move(stagingBuffer));
	}

	void GPUUploader::UploadImage(const ImageUploadSpec upload, vk::PipelineStageFlags2 dstStages, vk::AccessFlags2 dstAccess)
	{
		CDL_CORE_ASSERT(Platform::IsMainThread(), "GPUUploader owns its command pool, so it is main thread only");

		// Create staging buffer
		vk::DeviceSize totalSize = upload.Data.size_bytes();
		if (totalSize == 0)
			return;

		GPUBuffer stagingBuffer = m_Allocator.CreateBuffer({
			.Size = totalSize,
			.Usage = vk::BufferUsageFlagBits::eTransferSrc,
			.Flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
			.DebugName = "Image Staging"
			});

		// Upload image bytes (cpu) to a staging buffer
		[[maybe_unused]] VkResult result = vmaCopyMemoryToAllocation(m_Allocator.GetAllocator(), upload.Data.data(), stagingBuffer.GetAllocation(), 0, upload.Data.size_bytes());
		CDL_CORE_ASSERT(result == VK_SUCCESS, "Failed to write the staging buffer");

		// Wait for the previous upload to finish, then reset the command pool and begin a new command buffer
		m_RenderDevice.WaitFor(QueueType::Graphics, m_LastUploadValue);
		m_StagingDeletions.Drain(QueueType::Graphics, m_LastUploadValue);
		m_CommandPool.reset();
		m_CommandBuffer.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

		// Transition image layout to optimal layout for copy and copy from staging buffer to image
		vk::ImageSubresourceRange imageSubresourceRange = vk::ImageSubresourceRange{
			.aspectMask = vk::ImageAspectFlagBits::eColor,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
		};
		ImageState baseState{};
		ImageState copyState{
			.Layout = vk::ImageLayout::eTransferDstOptimal,
			.Access = vk::AccessFlagBits2::eTransferWrite,
			.Stage = vk::PipelineStageFlagBits2::eTransfer
		};
		vk::ImageMemoryBarrier2 barrier = MakeImageBarrier(upload.Destination, baseState, copyState, imageSubresourceRange);
		m_CommandBuffer.pipelineBarrier2({ .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier });

		// Copy from staging buffer to image
		const vk::BufferImageCopy2 region{    // rowLength/imageHeight 0 = tightly packed, which stb's output is
			.imageSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1 },
			.imageExtent = { upload.Extent.x, upload.Extent.y, 1 } 
		};
		m_CommandBuffer.copyBufferToImage2({ .srcBuffer = stagingBuffer.GetBuffer(), .dstImage = upload.Destination,
			.dstImageLayout = vk::ImageLayout::eTransferDstOptimal, .regionCount = 1, .pRegions = &region });

		// Transition image layout to destination layout
		ImageState finalState{
			.Layout = upload.DestinationLayout,
			.Access = dstAccess,
			.Stage = dstStages
		};
		barrier = MakeImageBarrier(upload.Destination, copyState, finalState, imageSubresourceRange);
		m_CommandBuffer.pipelineBarrier2({ .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier });

		m_CommandBuffer.end();

		// Submit command buffer and wait for completion
		vk::CommandBufferSubmitInfo commandBufferInfo{ .commandBuffer = *m_CommandBuffer };
		m_LastUploadValue = m_RenderDevice.Submit(QueueType::Graphics, { .CommandBuffers = { &commandBufferInfo, 1 } });

		// Free staging buffer
		m_StagingDeletions.Enqueue(QueueType::Graphics, m_LastUploadValue, std::move(stagingBuffer));
	}

	void GPUUploader::FreeCompletedStagingBuffers()
	{
		CDL_CORE_ASSERT(Platform::IsMainThread(), "GPUUploader is main thread only");
		m_StagingDeletions.Drain(QueueType::Graphics, m_RenderDevice.GetCompletedTimelineValue(QueueType::Graphics));
	}

}