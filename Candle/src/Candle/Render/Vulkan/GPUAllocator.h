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
		uint64_t SizeBytes{ 0 };
		uint64_t UsedBytes{ 0 };
		uint64_t UnusedBytes{ 0 };
	};

	struct GPUAllocatorStats
	{
		std::array<GPUHeapStats, VK_MAX_MEMORY_HEAPS> Heaps;
		uint32_t TotalUploads{ 0 };
		uint64_t TotalUploadBytes{ 0 };
		uint64_t CurrentVramUsageBytes{ 0 };	// The sum of the heaps' used bytes, for tuning the allocator's capacity
		uint64_t OsVramBudgetBytes{ 0 };	// The sum of the heaps' total bytes, for tuning the allocator's capacity
		uint64_t DeviceMaxVramBytes{ 0 };	// The sum of the heaps' total bytes, for tuning the allocator's capacity
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
		// Stat tracking
		struct UploadStats
		{
			// TODO: May need mutex since threading contract is any thread, 
			//  but the render thread is the only one that submits uploads and waits for them to finish so may not be necessary
			uint32_t Count{ 0 };
			uint64_t Bytes{ 0 };
		};

	private:
		GPUAllocatorSpec m_Spec;
		VmaAllocator m_Allocator;

		// Stat tracking
		UploadStats m_UploadStats;
	};

}