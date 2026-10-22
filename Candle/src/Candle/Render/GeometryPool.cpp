#include "cdlpch.h"
#include "GeometryPool.h"
#include "RenderConstants.h"
#include "RenderDevice.h"
#include "GPUUploader.h"

namespace Candle {

	GeometryPool::GeometryPool(GPUAllocator& allocator, GPUUploader& uploader, GPUResourceRegistry& registry)
		: m_Allocator(allocator)
		, m_Uploader(uploader)
		, m_Meshes(registry, kMaxMeshes)
	{
	}

	GPUMeshHandle GeometryPool::CreateMesh(std::span<const GPU::Vertex> vertices, std::span<const uint32_t> indices)
	{
		CDL_CORE_ASSERT(!vertices.empty() && !indices.empty(), "A mesh needs vertices and indices; Vulkan rejects zero-sized buffers");

		// Create buffers and device address before assigning to a slot (in case of any failures)
		GPUBuffer vertexBuffer = m_Allocator.CreateBuffer(GPUBufferSpec{
			.m_Size = vertices.size_bytes(),
			.m_Usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress
			});
		GPUBuffer indexBuffer = m_Allocator.CreateBuffer(GPUBufferSpec{
			.m_Size = indices.size_bytes(),
			.m_Usage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst
			});

		vk::DeviceAddress vertexAddress = vertexBuffer.GetDeviceAddress();

		// Vertices are read through their address in the vertex shader, indices by the input assembler
		const std::array<BufferUpload, 2> uploads = {
			BufferUpload{ .Data = std::as_bytes(vertices), .Destination = vertexBuffer.GetBuffer() },
			BufferUpload{ .Data = std::as_bytes(indices), .Destination = indexBuffer.GetBuffer() }
		};
		m_Uploader.UploadBuffers(uploads,
			vk::PipelineStageFlagBits2::eVertexShader | vk::PipelineStageFlagBits2::eIndexInput,
			vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eIndexRead);

		return m_Meshes.Insert(MeshEntry{
			.VertexBuffer = std::move(vertexBuffer),
			.IndexBuffer = std::move(indexBuffer),
			.VertexAddress = vertexAddress,
			.IndexCount = static_cast<uint32_t>(indices.size())
			});
	}

	void GeometryPool::ReleaseMesh(GPUMeshHandle handle)
	{
		m_Meshes.Release(handle);
	}

	bool GeometryPool::IsAlive(GPUMeshHandle handle) const
	{
		return m_Meshes.IsAlive(handle);
	}

	MeshDrawInfo GeometryPool::GetMeshInfo(GPUMeshHandle handle) const
	{
		const MeshEntry& mesh = m_Meshes.Get(handle);
		return MeshDrawInfo{
			.VerticesAddr = mesh.VertexAddress,
			.Indices = mesh.IndexBuffer.GetBuffer(),
			.IndexCount = mesh.IndexCount
		};
	}

}
