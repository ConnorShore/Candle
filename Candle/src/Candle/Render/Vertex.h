#pragma once

#include <glm/glm.hpp>

namespace Candle {

	// TODO: Move to an interop implementation to avoid duplicate implementatino between c++ and slang
	struct alignas(16) Vertex
	{
		glm::vec3 Position;
		float Padding; // Padding to align to 16 bytes
		glm::vec4 Color;
	};

}