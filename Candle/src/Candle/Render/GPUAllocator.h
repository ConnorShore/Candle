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

		GPUBuffer CreateBuffer(GPUBufferSpec spec);
		void DestroyBuffer(GPUBuffer& buffer);	// TODO: This will be handled by a deletion queue (or placed in a deletion queue)

		GPUImage CreateImage(GPUImageSpec spec);
		void DestroyImage(GPUImage& img);	// TODO: This will be handled by a deletion queue (or placed in a deletion queue)

		inline VmaAllocator& GetAllocator() { return m_Allocator; }
		inline RenderDevice* GetDevice() { return m_Spec.Device; }

	private:
		GPUAllocatorSpec m_Spec;
		VmaAllocator m_Allocator;
	};

}