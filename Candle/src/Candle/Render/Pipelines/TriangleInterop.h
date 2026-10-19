#pragma once

#include "Candle/Render/ShaderInterop/GlobalPushConstants.h"
#include "Candle/Render/ShaderInterop/Vertex.h"

CDL_INTEROP_BEGIN

	// TrianglePipeline's push-constant block; Global must stay the first member (see GlobalPushConstants)
	struct MeshDrawPushConstants
	{
		GlobalPushConstants Global;
		CDL_GPU_PTR(Vertex) Vertices;	// Set per draw
	};

CDL_INTEROP_END
