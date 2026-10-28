#pragma once

#include "Candle/Render/ShaderInterop/Vertex.h"

#include <vector>

namespace Candle {

	struct MeshData
	{
		std::vector<GPU::Vertex> Vertices;
		std::vector<uint32_t> Indices;
	};

}