#pragma once

#include "Asset.h"

namespace Candle {

	class IAssetHandler
	{
	public:
		virtual ~IAssetHandler() = default;
		virtual void OnLoad(Asset& asset) = 0;
		virtual void OnUnload(Asset& asset) = 0;
		virtual AssetType GetAssetType() const = 0;
	};

	template<IsAsset T>
	class AssetHandler : public IAssetHandler
	{
	public:
		AssetType GetAssetType() const final { return T::kType; }
		void OnLoad(Asset& asset) final { Load(static_cast<T&>(asset)); }
		void OnUnload(Asset& asset) final { Unload(static_cast<T&>(asset)); }

	protected:
		virtual void Load(T& asset) = 0;    // different names, since same-name overloads would hide the base's
		virtual void Unload(T& asset) = 0;
	};

}