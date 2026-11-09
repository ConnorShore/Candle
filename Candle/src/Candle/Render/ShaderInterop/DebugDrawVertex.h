#pragma once

#include "ShaderInterop.h"

CDL_INTEROP_BEGIN

struct DebugDrawVertex
{
	float3 Position;
	uint Color;		// RGBA8, red in the low byte as glm::packUnorm4x8 packs it
};

CDL_INTEROP_END