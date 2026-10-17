#pragma once

#include "RenderConstants.h"
#include "GPUBuffer.h"

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
		GPUAllocator(GPUAllocatorSpec& spec);
		~GPUAllocator();

		// Delete copy constructor and assignment operator
		GPUAllocator(const GPUAllocator&) = delete;
		GPUAllocator& operator=(const GPUAllocator&) = delete;

		// Move constructor and assignment operator
		GPUAllocator(GPUAllocator&& other) noexcept
			: m_Spec(std::move(other.m_Spec)), m_Allocator(other.m_Allocator)
		{
			other.m_Allocator = nullptr;
		}
		GPUAllocator& operator=(GPUAllocator&& other) noexcept
		{
			if (this != &other)
			{
				m_Spec = std::move(other.m_Spec);
				m_Allocator = other.m_Allocator;
				other.m_Allocator = nullptr;
			}
			return *this;
		}

		GPUBuffer CreateBuffer(GPUBufferSpec spec);

		inline VmaAllocator& GetAllocator() { return m_Allocator; }
		inline RenderDevice* GetDevice() { return m_Spec.Device; }

	private:
		GPUAllocatorSpec m_Spec;
		VmaAllocator m_Allocator;
	};

}