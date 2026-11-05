#pragma once

#include "Candle/Render/RenderConstants.h"
#include "Candle/Render/Vulkan/GPUBuffer.h"
#include "Candle/Render/Vulkan/GPUImage.h"

#include <vk_mem_alloc.h>
#include <array>

namespace Candle {

	class RenderDevice;
	class RenderInstance;

	struct GPUHeapStats
	{
		uint32_t HeapType{ 0 };	// VK_MEMORY_HEAP_DEVICE_LOCAL_BIT, etc.
		uint32_t BlockCount{ 0 };
		uint32_t AllocationCount{ 0 };
		uint64_t SizeBytes{ 0 };		// The physical heap
		uint64_t AllocationBytes{ 0 };	// What resources asked for
		uint64_t BlockBytes{ 0 };		// What VMA took from Vulkan; above AllocationBytes is unused space in its blocks
		uint64_t UsageBytes{ 0 };		// What the process uses; above BlockBytes is outside VMA, like the swapchain and pipelines
		uint64_t BudgetBytes{ 0 };		// What the OS will let the process use before it starts to struggle
	};

	struct GPUAllocatorStats
	{
		std::array<GPUHeapStats, VK_MAX_MEMORY_HEAPS> Heaps;
		uint32_t HeapCount{ 0 };				// How many entries of Heaps the device has
		bool HasMemoryBudget{ false };			// Without it, UsageBytes is just BlockBytes and BudgetBytes is 80% of the heap
		uint64_t CurrentVramUsageBytes{ 0 };	// UsageBytes across the device-local heaps
		uint64_t OsVramBudgetBytes{ 0 };		// BudgetBytes across the device-local heaps
		uint64_t DeviceMaxVramBytes{ 0 };		// SizeBytes across the device-local heaps
	};

	struct GPUAllocatorSpec
	{
		VulkanAPIVersion VulkanVersion{ kDefaultVulkanVersion };
		RenderDevice* Device{ nullptr };
		RenderInstance* Instance{ nullptr };
	};

	// Threading: any thread, since VMA locks internally unless created with VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT
	class GPUAllocator
	{
	public:
		GPUAllocator(GPUAllocatorSpec spec);
		~GPUAllocator();

		// Delete copy constructor and assignment operator
		GPUAllocator(const GPUAllocator&) = delete;
		GPUAllocator& operator=(const GPUAllocator&) = delete;

		// Delete move constructor and assignment operator
		GPUAllocator(GPUAllocator&&) = delete;
		GPUAllocator& operator=(GPUAllocator&&) = delete;

		// The result owns its memory and frees it when destroyed, so the caller destroys it only once the GPU is done with it
		GPUBuffer CreateBuffer(GPUBufferSpec spec);
		GPUImage CreateImage(GPUImageSpec spec);

		GPUAllocatorStats CaptureStats() const;

		void UpdateFrameCount(uint32_t frameCount);

		inline VmaAllocator& GetAllocator() { return m_Allocator; }
		inline RenderDevice* GetDevice() { return m_Spec.Device; }

	private:
		GPUAllocatorSpec m_Spec;
		VmaAllocator m_Allocator;
	};

}