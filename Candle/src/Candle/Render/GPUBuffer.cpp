#include "cdlpch.h"
#include "GPUBuffer.h"
#include "GPUAllocator.h"
#include "RenderDevice.h"

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

		if (spec.m_Usage & vk::BufferUsageFlagBits::eShaderDeviceAddress)
			m_DeviceAddress = allocator.GetDevice()->GetDevice().getBufferAddress({ .buffer = m_Buffer });
	}

	GPUBuffer::~GPUBuffer()
	{
		Destroy();
	}

	void GPUBuffer::Write(std::span<const std::byte> data, vk::DeviceSize offset)
	{
		CDL_CORE_ASSERT(m_Allocation, "Writing to an empty GPUBuffer");
		CDL_CORE_ASSERT(offset + data.size() <= m_Size, "GPUBuffer write out of bounds");

		VkResult result = vmaCopyMemoryToAllocation(m_Allocator, data.data(), m_Allocation, offset, data.size());
		CDL_CORE_ASSERT(result == VK_SUCCESS, "vmaCopyMemoryToAllocation failed; is the allocation host-visible?");
	}

	void GPUBuffer::Destroy()
	{
		// TODO: Instead of destroying the buffer immediately, we should defer destruction until the GPU is done using it.
		// This can be done by adding it to a destruction queue
		// Guarded because vmaDestroyBuffer asserts on a null allocator before checking for null handles
		if (m_Allocation)
			vmaDestroyBuffer(m_Allocator, m_Buffer, m_Allocation);
	}

}