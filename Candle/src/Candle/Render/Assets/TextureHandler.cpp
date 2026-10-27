#include "cdlpch.h"
#include "TextureHandler.h"

#include "Candle/Render/TexturePool.h"
#include "Candle/Render/TextureLoader.h"

namespace Candle {

	TextureHandler::TextureHandler(TexturePool& texturePool)
		: m_TexturePool(texturePool)
	{
	}

	void TextureHandler::Load(Texture& asset)
	{
		auto expectedInfo = TextureLoader::LoadTexture(asset.GetFilePath());
		if (!expectedInfo.has_value())
			return;

		const TextureLoadInfo& info = expectedInfo.value();
		TextureData& textureData = asset.m_TextureData;

		TextureCreateSpec spec{
			.Data = info.Data,
			.Extent = info.Extent,
			.Format = info.Format,
			.ColorSpace = textureData.ColorSpace
		};
		asset.m_GPUHandle = m_TexturePool.CreateTexture(std::move(spec));
	}

	void TextureHandler::Unload(Texture& asset)
	{
		m_TexturePool.ReleaseTexture(asset.GetGPUHandle());
		asset.m_GPUHandle = GPUTextureHandle{}; // Reset the handle to an invalid state
	}

}