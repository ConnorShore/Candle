#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <vk_mem_alloc.h>

namespace Candle {

	class GPUAllocator;

	struct GPUBufferSpec
	{
		size_t m_Size{ 0 };
		vk::BufferUsageFlags m_Usage{ };
		VmaAllocationCreateFlags m_Flags{ };
	};

	class GPUBuffer
	{
	public:
		GPUBuffer(const GPUBufferSpec& spec, GPUAllocator& allocator);
		~GPUBuffer();

		// Delete copy constructor and assignment operator
		GPUBuffer(const GPUBuffer&) = delete;
		GPUBuffer& operator=(const GPUBuffer&) = delete;

		// Move constructor and assignment operator
		GPUBuffer(GPUBuffer&& other) noexcept
			: m_Allocator(other.m_Allocator), m_Buffer(other.m_Buffer), m_Allocation(other.m_Allocation), m_Size(other.m_Size)
		{
			other.m_Buffer = nullptr;
			other.m_Allocation = nullptr;
			other.m_Size = 0;
		}
		inline GPUBuffer& operator=(GPUBuffer&& other) noexcept
		{
			if (this != &other)
			{
				m_Allocator = other.m_Allocator;
				m_Buffer = other.m_Buffer;
				m_Allocation = other.m_Allocation;
				m_Size = other.m_Size;
				other.m_Buffer = nullptr;
				other.m_Allocation = nullptr;
				other.m_Size = 0;
			}
			return *this;
		}

		inline vk::Buffer GetBuffer() const { return m_Buffer; }
		inline vk::DeviceSize GetSize() const { return m_Size; }
		inline VmaAllocation GetAllocation() const { return m_Allocation; }

	private:
		VmaAllocator m_Allocator{ nullptr };
		vk::Buffer m_Buffer{ nullptr };
		VmaAllocation m_Allocation{ nullptr };
		vk::DeviceSize m_Size{ 0 };
	};

}