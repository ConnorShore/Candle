#include "cdlpch.h"
#include "AssetManager.h"

#include <functional>

namespace Candle {

	uint64_t AssetManager::LoadAsset(SharedPtr<Asset> asset)
	{
		AssetType type = asset->GetType();
		std::string name = asset->GetName();
		uint64_t uuid = HashToUUID(name, type);

		// If there is an existing asset, unload it and replace it with new asset
		if (m_Assets.find(uuid) != m_Assets.end())
		{
			CDL_CORE_WARN(LogChannel::Asset, "Asset {} of type {} is already loaded. Unloading existing asset and replacing with new asset", name, static_cast<int>(type));
			UnloadAsset(name, type);
		}

		m_Assets[uuid] = std::move(asset);

		SharedPtr<Asset> savedAsset = m_Assets[uuid];

		// Call the appropriate asset handler for the asset type
		if (m_AssetHandlers.find(type) != m_AssetHandlers.end())
			m_AssetHandlers[type]->OnLoad(*savedAsset);
		else
			CDL_CORE_WARN(LogChannel::Asset, "No asset handler registered for asset type: {}", static_cast<int>(type));

		return uuid;
	}

	void AssetManager::UnloadAsset(const std::string& name, AssetType type)
	{
		uint64_t uuid = HashToUUID(name, type);

		auto assetIt = m_Assets.find(uuid);
		if (assetIt == m_Assets.end())
		{
			CDL_CORE_WARN(LogChannel::Asset, "No loaded asset named {} to unload", name);
			return;
		}

		if (m_AssetHandlers.find(type) != m_AssetHandlers.end())
			m_AssetHandlers[type]->OnUnload(*assetIt->second);
		else
			CDL_CORE_WARN(LogChannel::Asset, "No asset handler registered for asset type: {}", static_cast<int>(type));

		m_Assets.erase(assetIt);
	}

	void AssetManager::RegisterAssetHandler(ScopedPtr<IAssetHandler> handler)
	{
		m_AssetHandlers[handler->GetAssetType()] = std::move(handler);
	}

	uint64_t AssetManager::HashToUUID(std::string_view name, AssetType type) const
	{
		// TODO: Use much better hashing algorithm, this is just a placeholder for now
		std::string hashInput = std::string(name) + std::to_string(static_cast<int>(type));
		return std::hash<std::string_view>{}(hashInput);
	}

	SharedPtr<Asset> AssetManager::GetAsset(uint64_t uuid) const
	{
		if (m_Assets.find(uuid) != m_Assets.end())
			return m_Assets.at(uuid);
		else
			return nullptr;
	}

}