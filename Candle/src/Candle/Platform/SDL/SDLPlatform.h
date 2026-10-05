#pragma once

#include <SDL3/SDL.h>

namespace Candle::SDL {

	// Called for every raw event, in arrival order, inside Platform::PumpEvents, before translation.
	// Threading: main thread only. Set/clear outside PumpEvents. The hook must not pump events or destroy windows.
	using RawEventHook = void(*)(const SDL_Event& event, void* userData);
	void SetRawEventHook(RawEventHook hook, void* userData);        // nullptr clears the hook. Only one hook can be set at a time.

}