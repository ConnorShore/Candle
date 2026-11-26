#pragma once

#include "Candle/Render/Vulkan/GPUBuffer.h"

#include <vulkan/vulkan.hpp>

#include <string>
#include <cstdint>

namespace Candle {

	class GPUAllocator;

	struct GPUFrameAllocation
	{
		std::span<std::byte> Mapped;            // Write-only: write-combined memory is very slow to read back
		vk::DeviceAddress Address{ 0 };
		vk::Buffer Buffer{ nullptr };           // For BindIndexBuffer
		vk::DeviceSize Offset{ 0 };
	};

	// Threading: render thread only
	class GPUFrameArena
	{
	public:
		GPUFrameArena() = default;	// Empty, for a FrameContext slot filled later
		GPUFrameArena(GPUAllocator& allocator, uint32_t capacityBytes, const std::string& debugName = "GPUFrameArena");
		~GPUFrameArena();

		GPUFrameArena(const GPUFrameArena&) = delete;
		GPUFrameArena& operator=(const GPUFrameArena&) = delete;

		// Allocations already handed out stay valid: they point at the buffer, which moves with the arena
		GPUFrameArena(GPUFrameArena&&) noexcept = default;
		GPUFrameArena& operator=(GPUFrameArena&&) noexcept = default;

		GPUFrameAllocation Allocate(vk::DeviceSize size, vk::DeviceSize alignment);

		template<typename T> 
		inline GPUFrameAllocation Push(std::span<const T> data)
		{
			GPUFrameAllocation allocation = Allocate(data.size_bytes(), alignof(T));
			std::memcpy(allocation.Mapped.data(), data.data(), data.size_bytes());
			return allocation;
		}

		void Write(std::span<const std::byte> data, GPUFrameAllocation& allocation);

		void Reset();   // Only once the frame's RetireValue is reached
		void Flush();   // One flush of the used range before submit, for non-coherent memory

	private:
		GPUBuffer m_Buffer;
		vk::DeviceSize m_Capacity{ 0 };
		vk::DeviceSize m_Offset{ 0 };

		std::string m_DebugName;
	};

}