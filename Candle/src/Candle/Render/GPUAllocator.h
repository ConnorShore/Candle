#pragma once

#include "RenderConstants.h"
#include "GPUBuffer.h"
#include "GPUImage.h"

#include <vk_mem_alloc.h>

namespace Candle {

	class RenderDevice;
	class RenderInstance;

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

		inline VmaAllocator& GetAllocator() { return m_Allocator; }
		inline RenderDevice* GetDevice() { return m_Spec.Device; }

	private:
		GPUAllocatorSpec m_Spec;
		VmaAllocator m_Allocator;
	};

}