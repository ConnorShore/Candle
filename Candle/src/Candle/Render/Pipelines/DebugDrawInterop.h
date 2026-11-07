#pragma once

#include "Candle/Render/ShaderInterop/GlobalPushConstants.h"
#include "Candle/Render/ShaderInterop/DebugDrawVertex.h"

CDL_INTEROP_BEGIN

struct DebugDrawPushConstants
{
	GlobalPushConstants Global;
	CDL_GPU_PTR(DebugDrawVertex) Vertices;
};

CDL_INTEROP_END