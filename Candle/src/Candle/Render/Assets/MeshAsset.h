#pragma once

#include "Candle/Render/Assets/MeshData.h"
#include "Candle/Render/Resources/GPUHandles.h"
#include "Candle/Asset/Asset.h"

namespace Candle {

	// Threading: main thread only; MeshAssetHandler sets the GPU handle when the AssetManager loads it
	class MeshAsset : public Asset
	{
	public:
		// Built in code, so it has no file path
		MeshAsset(const std::string& name, MeshData&& meshData)
			: Asset(name, {}), m_MeshData(std::move(meshData)) {}

		// Kept after upload, since it is the only source a reload can upload from until meshes load from files
		inline const MeshData& GetMeshData() const { return m_MeshData; }
		inline const GPUMeshHandle& GetGPUHandle() const { return m_GPUHandle; }

		inline static constexpr AssetType kType = AssetType::Mesh;
		inline AssetType GetType() const override { return kType; }
	private:
		friend class MeshAssetHandler; // Allow the handler to modify the GPU handle

		MeshData m_MeshData;
		GPUMeshHandle m_GPUHandle;
	};

}