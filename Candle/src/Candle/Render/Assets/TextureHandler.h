#pragma once

#include "Texture.h"
#include "Candle/Asset/AssetHandler.h"

namespace Candle {

	class TexturePool;

	class TextureHandler : public AssetHandler<Texture>
	{
	public:
		TextureHandler(TexturePool& texturePool);
		virtual ~TextureHandler() = default;

	protected:
		void Load(Texture& asset) override;
		void Unload(Texture& asset) override;

	private:
		TexturePool& m_TexturePool;
	};

}