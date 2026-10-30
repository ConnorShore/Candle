#pragma once

namespace Candle {

	// The element in the bindless sampler array (set 0, binding 1), so shaders must index with the same values
	enum class SamplerType
	{
		LinearWrap,		// Trilinear + anisotropic, repeating: material textures on geometry
		LinearClamp,	// Trilinear, clamped: full-screen and UI passes, where repeating would bleed in the opposite edge
		PointWrap,		// Nearest, repeating: tiled noise and lookup textures whose texels must not blend
		PointClamp,		// Nearest, clamped: exact texel reads of render targets
		Count
	};

}