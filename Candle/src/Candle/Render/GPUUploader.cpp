#include "cdlpch.h"
#include "GPUUploader.h"
#include "RenderDevice.h"
#include "GPUAllocator.h"

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

		vk::CommandBufferAllocateInfo allocInfo{
			.commandPool = m_CommandPool,
			.level = vk::CommandBufferLevel::ePrimary,
			.commandBufferCount = 1
		};
		m_CommandBuffer = std::move(vk::raii::CommandBuffers(device, allocInfo).front());
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
			.m_Size = totalSize,
			.m_Usage = vk::BufferUsageFlagBits::eTransferSrc,
			.m_Flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
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

	void GPUUploader::FreeCompletedStagingBuffers()
	{
		CDL_CORE_ASSERT(Platform::IsMainThread(), "GPUUploader is main thread only");
		m_StagingDeletions.Drain(QueueType::Graphics, m_RenderDevice.GetCompletedTimelineValue(QueueType::Graphics));
	}

}