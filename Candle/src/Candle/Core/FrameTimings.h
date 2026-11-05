#pragma once

namespace Candle {

	// One thread's time for one iteration of its frame loop; the main and render threads each fill one per frame
	struct ThreadFrameTimings
	{
		float TotalMs{ 0.0f };		// The whole loop iteration
		float PumpWaitMs{ 0.0f };	// Blocked in the FramePump, waiting on the other thread

		inline float BusyMs() const { return TotalMs - PumpWaitMs; }
	};

}
