#pragma once

#include "Candle/Render/ShaderInterop/ShaderInterop.h"

CDL_INTEROP_BEGIN

	struct GuiVertex
	{
		float2 Position;
		float2 UV;
		uint Color;	// RGBA8 packed into a uint32_t
	};

CDL_INTEROP_END

