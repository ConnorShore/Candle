#include "cdlpch.h"
#include "GPUBuffer.h"
#include "GPUAllocator.h"
#include "RenderDevice.h"

namespace Candle {

	GPUBuffer::GPUBuffer(const GPUBufferSpec& spec, GPUAllocator& allocator)
		: m_Allocator(allocator.GetAllocator()), m_Size(spec.Size)
	{
		vk::BufferCreateInfo bufferInfo{ 
			.size = spec.Size, 
			.usage = spec.Usage, 
			.sharingMode = vk::SharingMode::eExclusive 
		};

		VmaAllocationCreateInfo allocInfo = {};
		allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
		allocInfo.flags = spec.Flags;

		VkResult result = vmaCreateBuffer(m_Allocator, reinterpret_cast<VkBufferCreateInfo*>(&bufferInfo), &allocInfo, reinterpret_cast<VkBuffer*>(&m_Buffer), &m_Allocation, nullptr);
		if (result != VK_SUCCESS)
			CDL_CORE_PANIC(LogChannel::Memory, "Failed to create GPU buffer of {} bytes: {}", spec.Size, vk::to_string(static_cast<vk::Result>(result)));

		if (!spec.DebugName.empty())
		{
			allocator.GetDevice()->SetDebugName(m_Buffer, spec.DebugName);
			vmaSetAllocationName(m_Allocator, m_Allocation, spec.DebugName.c_str());
		}

		if (spec.Usage & vk::BufferUsageFlagBits::eShaderDeviceAddress)
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
		// Immediate, so the owner keeps the buffer alive until the GPU is done with it (a DeletionQueue or a pool slot)
		// Guarded because vmaDestroyBuffer asserts on a null allocator before checking for null handles
		if (m_Allocation)
			vmaDestroyBuffer(m_Allocator, m_Buffer, m_Allocation);
	}

}