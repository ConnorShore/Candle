#pragma once

#include "Candle/Render/ShaderInterop/GlobalPushConstants.h"
#include "GuiVertex.h"

CDL_INTEROP_BEGIN

// GuiPipeline's push-constant block; Global must stay the first member (see GlobalPushConstants)
struct GuiDrawPushConstants
{
	GlobalPushConstants Global;
	CDL_GPU_PTR(GuiVertex) Vertices;	// Set per draw
	float2 Scale;
	float2 Translate;
	uint TextureIndex;
	uint SamplerIndex;
};

CDL_INTEROP_END