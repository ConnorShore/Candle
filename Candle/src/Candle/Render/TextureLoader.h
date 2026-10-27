#pragma once

#include "TextureData.h"

#include <filesystem>
#include <expected>

namespace Candle {

	struct TextureLoadInfo
	{
		glm::uvec2 Extent;
		TextureFormat Format;
		std::vector<std::byte> Data;
	};

	class TextureLoader
	{
	public:
		static std::expected<TextureLoadInfo, std::string> LoadTexture(const std::filesystem::path& filePath);
	};

}