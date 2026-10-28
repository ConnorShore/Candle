#pragma once

#include "MeshData.h"
#include "Candle/Render/GPUHandles.h"
#include "Candle/Asset/Asset.h"

namespace Candle {

	class MeshAsset : public Asset
	{
	public:
		MeshAsset(const std::string& name, MeshData&& meshData) 
			: Asset(name, "filepath.mesh"), m_MeshData(std::move(meshData)) {}

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