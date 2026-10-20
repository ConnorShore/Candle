#pragma once

#include "Candle/Render/ShaderInterop/ShaderInterop.h"

CDL_INTEROP_BEGIN

	// Per-frame data every shader reaches through GlobalPushConstants::Frame; one copy per frame in flight.
	// The frame's root table: variable-count data (lights, ...) joins as a pointer plus a count, never as an inline array.
	struct FrameData
	{
		float Time;		// Seconds, wrapped by the renderer so a float keeps its precision
	};

CDL_INTEROP_END
