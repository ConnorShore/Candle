#include "cdlpch.h"
#include "GPUAllocator.h"
#include "Candle/Render/Vulkan/RenderDevice.h"
#include "Candle/Render/Vulkan/RenderInstance.h"
#include "Candle/Render/Vulkan/VulkanUtils.h"

namespace Candle {

	GPUAllocator::GPUAllocator(GPUAllocatorSpec spec)
		: m_Spec(spec)
	{
		VmaAllocatorCreateInfo allocatorCreateInfo = {};
		allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
		if (m_Spec.Device->HasMemoryBudget())
			allocatorCreateInfo.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;	// VMA trusts this flag, so it must match the device
		allocatorCreateInfo.vulkanApiVersion = ToVulkanApiVersion(m_Spec.VulkanVersion);
		allocatorCreateInfo.physicalDevice = static_cast<VkPhysicalDevice>(*m_Spec.Device->GetPhysicalDevice());
		allocatorCreateInfo.device = static_cast<VkDevice>(*m_Spec.Device->GetDevice());
		allocatorCreateInfo.instance = static_cast<VkInstance>(*m_Spec.Instance->GetVulkanInstance());

		VkResult result = vmaCreateAllocator(&allocatorCreateInfo, &m_Allocator);
		if (result != VK_SUCCESS)
			CDL_CORE_PANIC(LogChannel::Memory, "Failed to create the GPU allocator: {}", vk::to_string(static_cast<vk::Result>(result)));
	}

	GPUAllocator::~GPUAllocator()
	{
		vmaDestroyAllocator(m_Allocator);
	}

	GPUBuffer GPUAllocator::CreateBuffer(GPUBufferSpec spec)
	{
		GPUBuffer ret{ spec, *this };

		m_UploadStats.Count++;
		m_UploadStats.Bytes += static_cast<uint64_t>(ret.GetSize());

		return ret;
	}

	GPUImage GPUAllocator::CreateImage(GPUImageSpec spec)
	{
		GPUImage ret{ spec, *this };

		m_UploadStats.Count++;
		m_UploadStats.Bytes += ret.GetSizeBytes();

		return ret;
	}

	GPUAllocatorStats GPUAllocator::CaptureStats() const
	{
		const VkPhysicalDeviceMemoryProperties* memoryProps{ nullptr };
		vmaGetMemoryProperties(m_Allocator, &memoryProps);

		VmaTotalStatistics stats;
		vmaCalculateStatistics(m_Allocator, &stats);

		std::array<GPUHeapStats, VK_MAX_MEMORY_HEAPS> heapStats{};
		for (uint32_t i = 0; i < memoryProps->memoryHeapCount; ++i)
		{
			VmaDetailedStatistics heapStatsVma = stats.memoryHeap[i];

			uint64_t totalBytes = static_cast<uint64_t>(heapStatsVma.statistics.blockBytes);
			uint64_t usedBytes = static_cast<uint64_t>(heapStatsVma.statistics.blockBytes - heapStatsVma.statistics.allocationBytes);
			uint64_t unusedBytes = totalBytes - usedBytes;
			heapStats[i] = GPUHeapStats{
				.HeapType = memoryProps->memoryHeaps[i].flags,
				.SizeBytes = totalBytes,
				.UsedBytes = usedBytes,
				.UnusedBytes = unusedBytes
			};
		}

		return GPUAllocatorStats{
			.Heaps = std::move(heapStats),
			.TotalUploads = m_UploadStats.Count,
			.TotalUploadBytes = m_UploadStats.Bytes
		};
	}

	void GPUAllocator::UpdateFrameCount(uint32_t frameCount)
	{
		vmaSetCurrentFrameIndex(m_Allocator, frameCount);
	}

}