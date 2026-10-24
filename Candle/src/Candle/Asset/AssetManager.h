#pragma once

#include "Asset.h"
#include "AssetHandler.h"
#include "Candle/Core/Memory/SharedPtr.h"
#include "Candle/Core/Memory/ScopedPtr.h"

#include <string>
#include <string_view>
#include <filesystem>
#include <unordered_map>

namespace Candle {

	// Temporary placeholder for loose structure
	class AssetManager
	{
	public:
		AssetManager() = default;
		~AssetManager() = default;

		// Temporary placeholder for now (creates default mesh for now)
		uint64_t LoadAsset(const std::string& name, SharedPtr<Asset> asset);
		void UnloadAsset(const std::string& name, AssetType type);

		// See if can enfore kType to be defined via Asset (IsAsset above should work partially but want compiler error if kType is not defined when creating a new asset class
		// Then GetType() can exist in base asset class and return kType (which is static) for the derived class
		template<IsAsset T>
		[[nodiscard]] inline SharedPtr<T> GetAsset(const std::string& name) const
		{
			SharedPtr<Asset> asset = GetAsset(HashToUUID(name, T::kType));
			CDL_CORE_ASSERT(!asset || asset->GetType() == T::kType, "AssetID collision");
			return StaticPointerCast<T>(asset);
		}

		[[nodiscard]] SharedPtr<Asset> GetAsset(uint64_t uuid) const;

		void RegisterAssetHandler(ScopedPtr<IAssetHandler> handler);

	private:
		// TODO: Probably move to some utility class for hashing
		uint64_t HashToUUID(std::string_view name, AssetType type) const;

	private:
		std::unordered_map<uint64_t, SharedPtr<Asset>> m_Assets;
		std::unordered_map<AssetType, ScopedPtr<IAssetHandler>> m_AssetHandlers;
	};

}