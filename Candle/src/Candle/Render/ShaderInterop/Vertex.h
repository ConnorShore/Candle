#pragma once

#include "Candle/Render/ShaderInterop/ShaderInterop.h"

CDL_INTEROP_BEGIN

	// The engine's vertex format; shaders pull it through a device address rather than vertex input
	struct Vertex
	{
		float3 Position;
		float UVx;
		float3 Normal;
		float UVy;
	};

CDL_INTEROP_END
