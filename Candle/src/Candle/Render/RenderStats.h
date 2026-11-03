#pragma once

#include "Candle/Core/FrameTimings.h"
#include "Candle/Render/Resources/TexturePool.h"

namespace Candle {

	// What the render thread reports about one completed frame. It rides back to the main thread in FramePacket::RenderStats,
	// so it needs no lock: the FramePump hand-off orders the render thread's write before the main thread's read.
	struct RenderStats
	{
		ThreadFrameTimings Timings;
		TexturePoolStats TextureStats;
	};

}
