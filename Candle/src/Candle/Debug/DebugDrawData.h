#pragma once

#include "Candle/Render/ShaderInterop/DebugDrawVertex.h"

#include <array>
#include <cstdint>
#include <vector>

namespace Candle {

	enum class DebugDrawBucket : uint8_t { WorldDepthTested, WorldNoDepth, Screen, Count };

	struct DebugDrawData
	{
		std::array<std::vector<GPU::DebugDrawVertex>, static_cast<size_t>(DebugDrawBucket::Count)> Buckets;
	};

}