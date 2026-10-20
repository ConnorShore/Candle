#include "cdlpch.h"
#include "GeometryPool.h"
#include "RenderDevice.h"
#include "GPUUploader.h"

namespace Candle {

	GeometryPool::GeometryPool(GPUAllocator& allocator, GPUUploader& uploader)
		: m_Allocator(allocator)
		, m_Uploader(uploader)
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

		// Find free slot and assing the mesh entry
		// TODO: This will need to change once we split to render thread.  If m_Slots changes location due to resize, the renderer may be accessing a stale pointer.
		uint32_t index = m_FirstFreeIndex;
		if (index == kInvalidIndex)
		{
			index = static_cast<uint32_t>(m_Slots.size());
			m_Slots.emplace_back();
		}
		else
		{
			m_FirstFreeIndex = m_Slots[index].NextFreeIndex;
		}

		Slot& slot = m_Slots[index];
		slot.NextFreeIndex = kInvalidIndex;
		slot.Mesh.emplace(std::move(vertexBuffer), std::move(indexBuffer), vertexAddress, static_cast<uint32_t>(indices.size()));

		return { .Index = index, .Generation = slot.Generation };
	}

	void GeometryPool::DestroyMesh(GPUMeshHandle handle)
	{
		// A stale handle would free the mesh that reused its slot, and push the slot onto the free list a second time
		CDL_CORE_ASSERT(IsAlive(handle), "DestroyMesh was given an invalid or already destroyed mesh handle");
		Slot& slot = m_Slots[handle.Index];
		slot.Mesh.reset();
		++slot.Generation;
		slot.NextFreeIndex = m_FirstFreeIndex;
		m_FirstFreeIndex = handle.Index;
	}

	MeshDrawInfo GeometryPool::GetMeshInfo(GPUMeshHandle handle)
	{
		CDL_CORE_ASSERT(IsAlive(handle), "GetMeshInfo was given an invalid or destroyed mesh handle");
		return MeshDrawInfo{
			.VerticesAddr = m_Slots[handle.Index].Mesh->VertexAddress,
			.Indices = m_Slots[handle.Index].Mesh->IndexBuffer.GetBuffer(),
			.IndexCount = m_Slots[handle.Index].Mesh->IndexCount
		};
	}

	bool GeometryPool::IsAlive(GPUMeshHandle handle) const
	{
		// DestroyMesh bumps the generation, so only handles issued since the slot was last filled match it
		return handle.Index < m_Slots.size()
			&& m_Slots[handle.Index].Generation == handle.Generation
			&& m_Slots[handle.Index].Mesh.has_value();
	}

}