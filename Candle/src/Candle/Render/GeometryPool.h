#pragma once

#include "GPUHandles.h"
#include "GPUBuffer.h"
#include "GPUAllocator.h"
#include "GPUResourcePool.h"

#include "Candle/Render/Assets/MeshDrawInfo.h"
#include "Candle/Render/ShaderInterop/Vertex.h"

#include <span>
#include <string>

namespace Candle {

	class GPUUploader;
	class GPUResourceRegistry;

	// Threading: CreateMesh, ReleaseMesh and IsAlive on the main thread; GetMeshInfo on the render thread.
	// A released mesh keeps its buffers until the GPU retires the frame that carried the release.
	class GeometryPool
	{
	public:
		GeometryPool(GPUAllocator& allocator, GPUUploader& uploader, GPUResourceRegistry& registry);
		~GeometryPool() = default;

		// Delete copy constructor and assignment operator
		GeometryPool(const GeometryPool&) = delete;
		GeometryPool& operator=(const GeometryPool&) = delete;

		GPUMeshHandle CreateMesh(std::span<const GPU::Vertex> vertices, std::span<const uint32_t> indices, const std::string& debugName = "Mesh");
		void ReleaseMesh(GPUMeshHandle handle);

		// False for the default handle and for a released mesh, even once another mesh reuses its slot
		bool IsAlive(GPUMeshHandle handle) const;

		MeshDrawInfo GetMeshInfo(GPUMeshHandle handle) const;

	private:
		struct MeshEntry
		{
			GPUBuffer VertexBuffer;
			GPUBuffer IndexBuffer;
			uint32_t IndexCount;
		};

	private:
		GPUAllocator& m_Allocator;
		GPUUploader& m_Uploader;
		GPUResourcePool<MeshEntry, MeshTag> m_Meshes;
	};

}
