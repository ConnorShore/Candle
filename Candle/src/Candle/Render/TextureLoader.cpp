#include "cdlpch.h"
#include "TextureLoader.h"

#include "Candle/Platform/Platform.h"

#include <stb_image.h>

namespace Candle {

	std::expected<TextureLoadInfo, std::string> TextureLoader::LoadTexture(const std::filesystem::path& filePath)
	{
		const auto texData = Platform::ReadFile(filePath);
		if (!texData.has_value())
		{
			CDL_CORE_ERROR(LogChannel::Asset, "Failed to load texture: {}", filePath.string());
			return std::unexpected("Failed to load texture!");
		}

		int width = 0, height = 0, fileChannels = 0;
		const stbi_uc* stbData = reinterpret_cast<const stbi_uc*>(texData.value().data());
		stbi_uc* pixels = stbi_load_from_memory(stbData, static_cast<int>(texData->size()), &width, &height, &fileChannels, STBI_rgb_alpha);
		if (!pixels)
			return std::unexpected(std::format("Failed to decode '{}': {}", filePath.string(), stbi_failure_reason()));

		const auto* first = reinterpret_cast<const std::byte*>(pixels);
		std::vector<std::byte> rgba(first, first + static_cast<size_t>(width) * height * 4);
		stbi_image_free(pixels);
		return TextureLoadInfo{ 
			.Extent = { width, height }, 
			.Format = TextureFormat::eRgba8, 
			.Data = std::move(rgba) 
		};
	}

}