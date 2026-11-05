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
		return { spec, *this };
	}

	GPUImage GPUAllocator::CreateImage(GPUImageSpec spec)
	{
		return { spec, *this };
	}

	GPUAllocatorStats GPUAllocator::CaptureStats() const
	{
		const VkPhysicalDeviceMemoryProperties* memoryProps{ nullptr };
		vmaGetMemoryProperties(m_Allocator, &memoryProps);

		// Not vmaCalculateStatistics: it walks every block, and this runs every frame
		std::array<VmaBudget, VK_MAX_MEMORY_HEAPS> budgets{};
		vmaGetHeapBudgets(m_Allocator, budgets.data());

		GPUAllocatorStats stats{
			.Heaps = {},
			.HeapCount = memoryProps->memoryHeapCount,
			.HasMemoryBudget = m_Spec.Device->HasMemoryBudget()
		};

		for (uint32_t i = 0; i < stats.HeapCount; ++i)
		{
			const VkMemoryHeap& heap = memoryProps->memoryHeaps[i];
			const VmaBudget& budget = budgets[i];
			stats.Heaps[i] = GPUHeapStats{
				.HeapType = heap.flags,
				.BlockCount = budget.statistics.blockCount,
				.AllocationCount = budget.statistics.allocationCount,
				.SizeBytes = heap.size,
				.AllocationBytes = budget.statistics.allocationBytes,
				.BlockBytes = budget.statistics.blockBytes,
				.UsageBytes = budget.usage,
				.BudgetBytes = budget.budget
			};

			if (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
			{
				stats.CurrentVramUsageBytes += budget.usage;
				stats.OsVramBudgetBytes += budget.budget;
				stats.DeviceMaxVramBytes += heap.size;
			}
		}

		return stats;
	}

	void GPUAllocator::UpdateFrameCount(uint32_t frameCount)
	{
		vmaSetCurrentFrameIndex(m_Allocator, frameCount);
	}

}