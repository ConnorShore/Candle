#pragma once

#include "Candle/Core/FrameTimings.h"
#include "Candle/Render/Vulkan/GPUAllocator.h"
#include "Candle/Render/Resources/GPUResourcePool.h"
#include "Candle/Render/Resources/GPUUploader.h"

namespace Candle {

	struct GPUResourceStats
	{
		GPUResourcePoolStats GeometryPoolStats;
		GPUResourcePoolStats TexturePoolStats;
		GPUAllocatorStats AllocatorStats;
		GPUUploaderStats UploaderStats;
		uint64_t GraphicsTimelineLag{ 0 };	// Graphics submits the GPU hasn't finished; counts uploads as well as frames
	};

	// What the render thread reports about one completed frame. It rides back to the main thread in FramePacket::RenderStats,
	// so it needs no lock: the FramePump hand-off orders the render thread's write before the main thread's read.
	struct RenderStats
	{
		ThreadFrameTimings Timings;
	};

}
