#include "cdlpch.h"
#include "GPUBuffer.h"
#include "GPUAllocator.h"

namespace Candle {

	GPUBuffer::GPUBuffer(const GPUBufferSpec& spec, GPUAllocator& allocator)
		: m_Allocator(allocator.GetAllocator()), m_Size(spec.m_Size)
	{
		vk::BufferCreateInfo bufferInfo{ 
			.size = spec.m_Size, 
			.usage = spec.m_Usage, 
			.sharingMode = vk::SharingMode::eExclusive 
		};

		VmaAllocationCreateInfo allocInfo = {};
		allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
		// TODO: Add support for allocInfo.flags to specify memory properties, e.g., VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT for staging buffers

		vmaCreateBuffer(m_Allocator, reinterpret_cast<VkBufferCreateInfo*>(&bufferInfo), &allocInfo, reinterpret_cast<VkBuffer*>(&m_Buffer), &m_Allocation, nullptr);
	}

	GPUBuffer::~GPUBuffer()
	{
		vmaDestroyBuffer(m_Allocator, m_Buffer, m_Allocation);
	}

}