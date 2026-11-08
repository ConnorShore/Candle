#pragma once

#include "Candle/Render/ShaderInterop/DebugDrawVertex.h"

#include <cstdint>
#include <vector>

namespace Candle {

	enum class DebugDrawBucket : uint8_t { WorldDepthTested, WorldNoDepth, Screen, Count };

	// A run of vertices sharing a bucket and line width, so one draw covers it
	struct DebugDrawBatch
	{
		DebugDrawBucket Bucket;
		float LineWidth;
		uint32_t FirstVertex;
		uint32_t VertexCount;
	};

	struct DebugDrawData
	{
		std::vector<GPU::DebugDrawVertex> Vertices;
		std::vector<DebugDrawBatch> Batches;	// Sorted by bucket, then line width
	};

}