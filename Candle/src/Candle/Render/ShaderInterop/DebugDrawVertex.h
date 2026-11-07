#pragma once

#include "ShaderInterop.h"

CDL_INTEROP_BEGIN

struct DebugDrawVertex
{
	float3 Position;
	uint16_t Color;		// RGBA8 packed into a 16-bit unsigned integer
};

CDL_INTEROP_END