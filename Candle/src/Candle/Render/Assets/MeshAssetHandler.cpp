#include "cdlpch.h"
#include "MeshAssetHandler.h"
#include "Candle/Render/Assets/MeshAsset.h"

#include "Candle/Render/Resources/GeometryPool.h"

namespace Candle {

	MeshAssetHandler::MeshAssetHandler(GeometryPool& geometryPool)
		: m_GeometryPool(geometryPool)
	{
	}

	void MeshAssetHandler::Load(MeshAsset& asset)
	{
		const MeshData& meshData = asset.GetMeshData();
		asset.m_GPUHandle = m_GeometryPool.CreateMesh(meshData.Vertices, meshData.Indices, asset.GetName());
	}

	void MeshAssetHandler::Unload(MeshAsset& asset)
	{
		m_GeometryPool.ReleaseMesh(asset.GetGPUHandle());
		asset.m_GPUHandle = GPUMeshHandle{}; // Reset the handle to an invalid state
	}

}