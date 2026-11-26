#include "cdlpch.h"
#include "GPUArenaAllocator.h"

#include "Candle/Render/Vulkan/GPUAllocator.h"

namespace Candle {


	GPUFrameArena::GPUFrameArena(GPUAllocator& allocator, uint32_t capacityBytes, const std::string& debugName/* = "GPUFrameArena"*/)
		: m_Capacity(capacityBytes), m_Offset(0), m_DebugName(debugName)
	{
		m_Buffer = allocator.CreateBuffer({
			.Size = capacityBytes,
			.Usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
			.Flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
			.DebugName = debugName 
		});
	}

	GPUFrameArena::~GPUFrameArena()
	{
	}

	GPUFrameAllocation GPUFrameArena::Allocate(vk::DeviceSize size, vk::DeviceSize alignment)
	{
		CDL_CORE_ASSERT(size > 0, "GPUFrameArena [{}] allocation size must be greater than 0", m_DebugName);

		// TODO NOW: Determine if this is needed
		if (alignment < 16)
		{
			alignment = 16;	// Minimum alignment for a storage buffer
			//CDL_CORE_WARN(LogChannel::Render, "GPUFrameArena [{}] allocation size {} is not aligned to 16 bytes; aligning it", m_DebugName, size);
		}

		// Align the offset to the requested alignment
		m_Offset = (m_Offset + alignment - 1) & ~(alignment - 1);	// Align the offset

		// Ensure we have enough space in the arena for the allocation
		CDL_CORE_ASSERT(m_Offset + size <= m_Capacity, "GPUFrameArena [{}] out of memory: {} more bytes with {} of {} used; raise its capacity", m_DebugName, size, m_Offset, m_Capacity);
		
		GPUFrameAllocation allocation{
			.Mapped = m_Buffer.GetMappedMemory(m_Offset, size),
			.Address = m_Buffer.GetDeviceAddress() + m_Offset,
			.Buffer = m_Buffer.GetBuffer(),
			.Offset = m_Offset
		};

		// Update the offset for the next allocation
		m_Offset += size;

		return allocation;
	}

	void GPUFrameArena::Write(std::span<const std::byte> data, GPUFrameAllocation& allocation)
	{
		CDL_CORE_ASSERT(allocation.Mapped.size() >= data.size(), "GPUFrameArena write out of bounds");
		std::memcpy(allocation.Mapped.data(), data.data(), data.size());
	}

	void GPUFrameArena::Reset()
	{
		m_Offset = 0;
	}

	void GPUFrameArena::Flush()
	{
		m_Buffer.FlushRange(0, m_Offset);	// Flush the used range before submit, for non-coherent memory
	}

}