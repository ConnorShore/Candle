#pragma once

#include "Texture.h"
#include "Candle/Asset/AssetHandler.h"

namespace Candle {

	class TexturePool;
	class BindlessTable;

	class TextureHandler : public AssetHandler<Texture>
	{
	public:
		TextureHandler(TexturePool& texturePool, BindlessTable& bindlessTable);
		virtual ~TextureHandler() = default;

	protected:
		void Load(Texture& asset) override;
		void Unload(Texture& asset) override;

	private:
		TexturePool& m_TexturePool;
		BindlessTable& m_BindlessTable;
	};

}