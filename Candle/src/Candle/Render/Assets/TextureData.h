#pragma once

#include "Candle/Render/Vulkan/SamplerType.h"

#include "Candle/Core/Core.h"

#include <glm/glm.hpp>

#include <cstdint>

namespace Candle {


	enum class TextureColorSpace
	{
		Srgb,
		Linear,
	};

	// Only formats every Vulkan device must sample with linear filtering, so RGB data is expanded to RGBA8, as stb does.
	// R8 and RG8 hold data such as masks or normal XY, and their sRGB variants are optional, so they are linear only.
	enum class TextureFormat
	{
		Rgba8,
		Rg8,
		R8
	};

	// Engine textures TexturePool creates before any other, in this order, so each one's bindless index is its value
	enum class DefaultTexture : uint32_t
	{
		Error,	// What a missing or failed texture resolves to
		Count
	};

	// Texture data is tightly packed: rows of Extent.x texels, with no padding between them
	inline uint32_t BytesPerTexel(TextureFormat format)
	{
		switch (format)
		{
		case TextureFormat::Rgba8: return 4;
		case TextureFormat::Rg8:   return 2;
		case TextureFormat::R8:    return 1;
		}

		CDL_CORE_ASSERT(false, "Unhandled TextureFormat");
		return 0;
	}

	struct TextureData
	{
		SamplerType Sampler;
		TextureColorSpace ColorSpace;
		TextureFormat Format;
		glm::uvec2 Extent;
	};

}