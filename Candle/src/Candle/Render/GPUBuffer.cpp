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
		allocInfo.flags = spec.m_Flags;

		vmaCreateBuffer(m_Allocator, reinterpret_cast<VkBufferCreateInfo*>(&bufferInfo), &allocInfo, reinterpret_cast<VkBuffer*>(&m_Buffer), &m_Allocation, nullptr);
	}

	GPUBuffer::~GPUBuffer()
	{
		// TODO: Instead of destroying the buffer immediately, we should defer destruction until the GPU is done using it. 
		// This can be done by adding it to a destruction queue
		vmaDestroyBuffer(m_Allocator, m_Buffer, m_Allocation);
	}

}