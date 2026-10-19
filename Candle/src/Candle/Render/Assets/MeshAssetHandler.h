#pragma once

#include "MeshAsset.h"
#include "Candle/Asset/AssetHandler.h"

namespace Candle {

	class GeometryPool;

	class MeshAssetHandler : public AssetHandler<MeshAsset>
	{
	public:
		MeshAssetHandler(GeometryPool& geometryPool);
		virtual ~MeshAssetHandler() = default;

	protected:
		void Load(MeshAsset& asset) override;
		void Unload(MeshAsset& asset) override;

	private:
		GeometryPool& m_GeometryPool;
	};

}