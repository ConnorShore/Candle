#pragma once

#include "Sampler.h"	// TODO: May be worth moving SampleType into this file

#include <glm/glm.hpp>

namespace Candle {


	enum class TextureColorSpace
	{
		eSrgb,
		eLinear,
	};

	enum class TextureFormat
	{
		eRgba8,
		eRgb8,
		eRg8,
		eR8
	};

	struct TextureData
	{
		SamplerType Sampler;
		TextureColorSpace ColorSpace;
		TextureFormat Format;
		glm::uvec2 Extent;
	};

}