#pragma once

#include "Candle/Render/TextureData.h"
#include "Candle/Asset/Asset.h"
#include "Candle/Render/GPUHandles.h"

#include <glm/glm.hpp>

namespace Candle {

	// Threading: main thread only; TextureHandler fills in its data and GPU handle when the AssetManager loads it
	class Texture : public Asset
	{
	public:
		// For AssetManager::LoadAsset<Texture>; the TextureHandler fills in the data on load
		Texture(const std::string& name, const std::filesystem::path& filePath)
			: Asset(name, filePath), m_TextureData{} {}
		Texture(const std::string& name, const std::filesystem::path& filePath, TextureData&& textureData)
			: Asset(name, filePath), m_TextureData(std::move(textureData)) {}

		inline const TextureData& GetTextureData() const { return m_TextureData; }
		inline const GPUTextureHandle& GetGPUHandle() const { return m_GPUHandle; }

		inline static constexpr AssetType kType = AssetType::Texture;
		inline AssetType GetType() const override { return kType; }

	private:
		friend class TextureHandler;

		TextureData m_TextureData;
		GPUTextureHandle m_GPUHandle;
	};

}