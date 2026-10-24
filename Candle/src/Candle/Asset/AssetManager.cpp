#include "cdlpch.h"
#include "AssetManager.h"

#include <functional>

namespace Candle {

	uint64_t AssetManager::LoadAsset(const std::string& name, SharedPtr<Asset> asset)
	{
		AssetType type = asset->GetType();
		uint64_t uuid = HashToUUID(name, type);
		m_Assets[uuid] = asset;

		// Call the appropriate asset handler for the asset type
		if (m_AssetHandlers.find(type) != m_AssetHandlers.end())
			m_AssetHandlers[type]->OnLoad(*asset);
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