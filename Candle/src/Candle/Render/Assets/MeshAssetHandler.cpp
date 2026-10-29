#include "cdlpch.h"
#include "MeshAssetHandler.h"
#include "MeshAsset.h"

#include "Candle/Render/GeometryPool.h"

namespace Candle {

	MeshAssetHandler::MeshAssetHandler(GeometryPool& geometryPool)
		: m_GeometryPool(geometryPool)
	{
	}

	void MeshAssetHandler::Load(MeshAsset& asset)
	{
		const MeshData& meshData = asset.GetMeshData();
		asset.m_GPUHandle = m_GeometryPool.CreateMesh(meshData.Vertices, meshData.Indices, asset.GetName());

		// The upload copied it into a staging buffer, so keeping the CPU copy would double the mesh in memory
		asset.m_MeshData = MeshData{};
	}

	void MeshAssetHandler::Unload(MeshAsset& asset)
	{
		m_GeometryPool.ReleaseMesh(asset.GetGPUHandle());
		asset.m_GPUHandle = GPUMeshHandle{}; // Reset the handle to an invalid state
	}

}