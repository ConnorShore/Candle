#pragma once

#include "Candle/Render/ShaderInterop/ShaderInterop.h"

CDL_INTEROP_BEGIN

	// Per-frame data every shader reaches through GlobalPushConstants::Frame; one copy per frame in flight.
	// The frame's root table: variable-count data (lights, ...) joins as a pointer plus a count, never as an inline array.
	struct FrameData
	{
		float4x4 ViewProjection;				// The view-projection matrix for this frame; the renderer uses this to transform vertices to clip space
		CDL_GPU_PTR(float4x4) TransformData;	// Pointer to the array of model matrices for this frame's draw items; the renderer writes them into a GPU buffer and shaders read them by address
		uint TransformCount;					// The number of model matrices in TransformData; the renderer writes this and shaders read it
		float Time;								// Seconds, wrapped by the renderer so a float keeps its precision
	};

CDL_INTEROP_END
