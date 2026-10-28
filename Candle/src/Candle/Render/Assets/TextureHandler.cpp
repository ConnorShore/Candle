#include "cdlpch.h"
#include "TextureHandler.h"

#include "Candle/Render/TexturePool.h"
#include "Candle/Render/TextureLoader.h"
#include "Candle/Render/BindlessTable.h"

namespace Candle {

	TextureHandler::TextureHandler(TexturePool& texturePool, BindlessTable& bindlessTable)
		: m_TexturePool(texturePool), m_BindlessTable(bindlessTable)
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
		TextureInfo texInfo = m_TexturePool.GetTextureInfo(asset.m_GPUHandle);

		// Write the texture's image view to the bindless table at the index corresponding to its GPU handle
		m_BindlessTable.WriteTexture(texInfo.Index, texInfo.ImageView);
	}

	void TextureHandler::Unload(Texture& asset)
	{
		m_TexturePool.ReleaseTexture(asset.GetGPUHandle());
		asset.m_GPUHandle = GPUTextureHandle{}; // Reset the handle to an invalid state
	}

}