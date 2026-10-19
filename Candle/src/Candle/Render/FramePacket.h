#pragma once

#include "GPUHandles.h"

#include <vector>

namespace Candle {

	// A packet of data that is sent to the renderer for rendering. This will be filled by the simulation and then sent to the render thread for rendering.
	// Threading: simulation thread fills it, render thread reads it. The simulation thread must not modify the packet after it has been sent to the render thread.
	struct FramePacket
	{
		// Vector not span as data is copied to the render thread, and the simulation thread may modify its own copy of the vector after sending it to the render thread
		std::vector<GPUMeshHandle> Meshes;
	};

}