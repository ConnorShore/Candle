#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <vk_mem_alloc.h>

#include <span>
#include <string>

namespace Candle {

	class GPUAllocator;

	struct GPUBufferSpec
	{
		size_t Size{ 0 };
		vk::BufferUsageFlags Usage{ };
		VmaAllocationCreateFlags Flags{ };
		std::string DebugName;	// Names the buffer and its allocation; empty leaves both unnamed
	};

	// Threading: not internally synchronized; one owner at a time, which may hand it to another thread
	class GPUBuffer
	{
	public:
		GPUBuffer() = default;	// Empty, for a slot filled later (e.g. FrameContext)
		GPUBuffer(const GPUBufferSpec& spec, GPUAllocator& allocator);
		~GPUBuffer();

		// Delete copy constructor and assignment operator
		GPUBuffer(const GPUBuffer&) = delete;
		GPUBuffer& operator=(const GPUBuffer&) = delete;

		// Move constructor and assignment operator
		GPUBuffer(GPUBuffer&& other) noexcept
			: m_Allocator(other.m_Allocator), m_Buffer(other.m_Buffer), m_Allocation(other.m_Allocation), m_Size(other.m_Size), m_DeviceAddress(other.m_DeviceAddress)
		{
			other.m_Buffer = nullptr;
			other.m_Allocation = nullptr;
			other.m_Size = 0;
			other.m_DeviceAddress = 0;
		}
		inline GPUBuffer& operator=(GPUBuffer&& other) noexcept
		{
			if (this != &other)
			{
				Destroy();	// Otherwise the buffer being overwritten leaks
				m_Allocator = other.m_Allocator;
				m_Buffer = other.m_Buffer;
				m_Allocation = other.m_Allocation;
				m_Size = other.m_Size;
				m_DeviceAddress = other.m_DeviceAddress;
				other.m_Buffer = nullptr;
				other.m_Allocation = nullptr;
				other.m_Size = 0;
				other.m_DeviceAddress = 0;
			}
			return *this;
		}

		// Copies into the buffer, flushing if its memory isn't host-coherent. Needs a host-visible allocation (a
		// VMA_ALLOCATION_CREATE_HOST_ACCESS_* flag). Not synchronized: the caller guarantees the GPU isn't reading the range.
		void Write(std::span<const std::byte> data, vk::DeviceSize offset = 0);

		inline vk::Buffer GetBuffer() const { return m_Buffer; }
		inline vk::DeviceSize GetSize() const { return m_Size; }
		inline VmaAllocation GetAllocation() const { return m_Allocation; }
		inline vk::DeviceAddress GetDeviceAddress() const { return m_DeviceAddress; }	// 0 unless created with eShaderDeviceAddress

	private:
		void Destroy();

	private:
		VmaAllocator m_Allocator{ nullptr };
		vk::Buffer m_Buffer{ nullptr };
		VmaAllocation m_Allocation{ nullptr };
		vk::DeviceSize m_Size{ 0 };
		vk::DeviceAddress m_DeviceAddress{ 0 };
	};

}