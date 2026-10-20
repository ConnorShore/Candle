#pragma once

#include "Candle/Render/ShaderInterop/Vertex.h"

#include <vector>

namespace Candle {

	// Temporary placeholder for a mesh asset. Will eventually be a full asset with vertex/index buffers, materials, etc.
	struct MeshData
	{
		std::vector<GPU::Vertex> Vertices;
		std::vector<uint32_t> Indices;
	};

}