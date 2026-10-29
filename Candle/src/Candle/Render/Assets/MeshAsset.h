#pragma once

#include "MeshData.h"
#include "Candle/Render/GPUHandles.h"
#include "Candle/Asset/Asset.h"

namespace Candle {

	// Threading: main thread only; MeshAssetHandler sets the GPU handle when the AssetManager loads it
	class MeshAsset : public Asset
	{
	public:
		// Built in code, so it has no file path
		MeshAsset(const std::string& name, MeshData&& meshData)
			: Asset(name, {}), m_MeshData(std::move(meshData)) {}

		inline const MeshData& GetMeshData() const { return m_MeshData; }	// Empty once MeshAssetHandler has uploaded it
		inline const GPUMeshHandle& GetGPUHandle() const { return m_GPUHandle; }

		inline static constexpr AssetType kType = AssetType::Mesh;
		inline AssetType GetType() const override { return kType; }
	private:
		friend class MeshAssetHandler; // Allow the handler to set the GPU handle and drop the uploaded data

		MeshData m_MeshData;
		GPUMeshHandle m_GPUHandle;
	};

}