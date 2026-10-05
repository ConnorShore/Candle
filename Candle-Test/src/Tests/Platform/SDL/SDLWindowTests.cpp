// Window lifetime as SDL sees it: counts SDL's native windows directly. Opens real OS windows, so
// these are the Display type.

#include "TestFramework.h"
#include "TestHelpers.h"

#include <SDL3/SDL.h>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Display;

namespace {

	int NativeWindowCount()
	{
		int count = 0;
		SDL_free(SDL_GetWindows(&count));
		return count;
	}

}

CDL_TEST_CASE(SDLWindow, OwnsOneNativeWindowForItsLifetime, Display)
{
	WindowingBackend backend;
	{
		Window window(TestWindowSpec());
		CDL_EXPECT_EQ(NativeWindowCount(), 1);
	}
	CDL_EXPECT_EQ(NativeWindowCount(), 0);
}

// The destructor calls Destroy as well, so an explicit Destroy must leave the window safe to destroy again.
CDL_TEST_CASE(SDLWindow, DestroyIsIdempotent, Display)
{
	WindowingBackend backend;
	Window window(TestWindowSpec());

	window.Destroy();
	CDL_EXPECT_EQ(NativeWindowCount(), 0);
	window.Destroy();
}
