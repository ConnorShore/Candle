#pragma once

#include "Candle/Render/ShaderInterop/GlobalPushConstants.h"

CDL_INTEROP_BEGIN

// FrameGraphPreviewPipeline's push-constant block; Global must stay the first member (see GlobalPushConstants)
struct FrameGraphPreviewPushConstants
{
	GlobalPushConstants Global;
	uint TextureIndex;
	uint SamplerIndex;
	float2 Range;	// Remapped to black..white
	uint IsDepth;
};

CDL_INTEROP_END
