#pragma once

#include "Candle/Render/ShaderInterop/ShaderInterop.h"
#include "Candle/Render/ShaderInterop/FrameData.h"

CDL_INTEROP_BEGIN

	// The engine-wide head of every push-constant block. Vulkan allows one block per entry point, so each pipeline's
	// push struct embeds this as its first member, named Global; Pipeline checks that from both the C++ and shader side.
	struct GlobalPushConstants
	{
		CDL_GPU_PTR(FrameData) Frame;	// This frame in flight's FrameData
	};

CDL_INTEROP_END
