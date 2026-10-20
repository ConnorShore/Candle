#pragma once

#include "GPUHandles.h"
#include "GPUBuffer.h"
#include "GPUAllocator.h"

#include "Candle/Render/Assets/MeshDrawInfo.h"
#include "Candle/Render/ShaderInterop/Vertex.h"

#include <vector>
#include <optional>
#include <span>

namespace Candle {

	class GPUUploader;

	class GeometryPool
	{
	public:
		GeometryPool(GPUAllocator& allocator, GPUUploader& uploader);
		~GeometryPool() = default;

		// Delete copy constructor and assignment operator
		GeometryPool(const GeometryPool&) = delete;
		GeometryPool& operator=(const GeometryPool&) = delete;

		// Default move constructor and assignment operator
		GeometryPool(GeometryPool&&) = default;
		GeometryPool& operator=(GeometryPool&&) = default;

		GPUMeshHandle CreateMesh(std::span<const GPU::Vertex> vertices, std::span<const uint32_t> indices);
		void DestroyMesh(GPUMeshHandle handle);

		MeshDrawInfo GetMeshInfo(GPUMeshHandle handle);

		// False for the default handle and for one whose mesh was destroyed, even once another mesh reuses its slot
		bool IsAlive(GPUMeshHandle handle) const;

	private:
		struct Entry
		{
			GPUBuffer VertexBuffer;
			GPUBuffer IndexBuffer;
			vk::DeviceAddress VertexAddress;
			uint32_t IndexCount;
		};

		constexpr static uint32_t kInvalidIndex = std::numeric_limits<uint32_t>::max();
		struct Slot
		{
			std::optional<Entry> Mesh{ std::nullopt }; // Optional to allow for free slots
			uint32_t Generation{ 0 };
			uint32_t NextFreeIndex{ kInvalidIndex };
		};

	private:
		std::vector<Slot> m_Slots;	// Indexed by GPUMeshHandle.Index
		uint32_t m_FirstFreeIndex{ kInvalidIndex }; // Index of the first free slot in m_Slots

		GPUAllocator& m_Allocator;
		GPUUploader& m_Uploader;
	};

}