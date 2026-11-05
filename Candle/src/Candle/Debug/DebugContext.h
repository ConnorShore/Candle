#pragma once

#include "Candle/Core/Time.h"
#include "Candle/Core/FrameTimings.h"
#include "Candle/Render/RenderStats.h"
#include "Candle/Input/InputSnapshot.h"

namespace Candle {

	// What every debug panel is handed each frame. Snapshots are held by value: they are small, and nothing can dangle.
	// Threading: built and read on the main thread only.
	struct DebugContext
	{
		TimeStep DeltaTime;			// Frame to frame, which is what sets the frame rate. The threads overlap, so it isn't Main plus Render.
		InputSnapshot Input;		// This frame's input, the same capture OnUpdate gets; keys ImGui captured never reach it
		ThreadFrameTimings Main;	// The main (game) thread's last completed frame
		RenderStats Render;			// The render thread's last completed frame to reach the main thread; a few frames behind Main
		GPUResourceStats Resources;	// Captured on the main thread this frame, so unlike Render it isn't behind Main
	};

}
