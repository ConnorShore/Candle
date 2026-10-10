#pragma once

#include <array>

#include <glm/glm.hpp>

namespace Candle {

	// A packet of data that is sent to the renderer for rendering. This will be filled by the simulation and then sent to the render thread for rendering.
	// Threading: simulation thread fills it, render thread reads it. The simulation thread must not modify the packet after it has been sent to the render thread.
	struct FramePacket
	{
		// For now just vertex positions and colors for a triangle
		//std::array<glm::vec3, 3> VertexPositions;
		//std::array<glm::vec4, 3> VertexColors;
	};

}