#pragma once

#include "SamplerType.h"	// TODO: May be worth moving SampleType into this file

#include "Candle/Core/Core.h"

#include <glm/glm.hpp>

#include <cstdint>

namespace Candle {


	enum class TextureColorSpace
	{
		eSrgb,
		eLinear,
	};

	// Only formats every Vulkan device must sample with linear filtering, so RGB data is expanded to RGBA8, as stb does.
	// R8 and RG8 hold data such as masks or normal XY, and their sRGB variants are optional, so they are linear only.
	enum class TextureFormat
	{
		eRgba8,
		eRg8,
		eR8
	};

	// Texture data is tightly packed: rows of Extent.x texels, with no padding between them
	inline uint32_t BytesPerTexel(TextureFormat format)
	{
		switch (format)
		{
		case TextureFormat::eRgba8: return 4;
		case TextureFormat::eRg8:   return 2;
		case TextureFormat::eR8:    return 1;
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