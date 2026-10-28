#pragma once

// Compiled as C++ by the texture tests and as Slang by Candle-Test/res/shaders/src/BindlessFixture.slang, which
// includes it by relative path. Changing it means recompiling the fixture with Build.py shaders.

#include "Candle/Render/ShaderInterop/GlobalPushConstants.h"

CDL_INTEROP_BEGIN

	// One probe per draw: the fragment shader returns textures[TextureIndex] sampled through samplers[SamplerIndex] at UV.
	// Starts with Global, so Pipeline accepts it as a push-constant block; the shader never reads it.
	struct BindlessFixturePushConstants
	{
		GlobalPushConstants Global;
		float2 UV;
		uint TextureIndex;
		uint SamplerIndex;
	};

CDL_INTEROP_END
