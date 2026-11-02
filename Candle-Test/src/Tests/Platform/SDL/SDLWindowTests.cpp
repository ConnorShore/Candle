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

// The scale of the display the window is on, which GuiSystem sizes ImGui's style and fonts by.
CDL_TEST_CASE(SDLWindow, ContentScaleIsThatOfTheWindowsDisplay, Display)
{
	WindowingBackend backend;
	const Window window(TestWindowSpec());

	SDL_Window* native = reinterpret_cast<SDL_Window*>(window.GetNativeHandle());
	const float expected = SDL_GetDisplayContentScale(SDL_GetDisplayForWindow(native));
	CDL_NOTE(std::format("content scale {}", window.GetContentScale()));
	CDL_EXPECT_GT(window.GetContentScale(), 0.0f);
	CDL_EXPECT_EQ(window.GetContentScale(), expected);
}

// The constructor asks before its window exists, and a destroyed window has none either, so both fall back to the primary
// display rather than handing SDL a null or dangling window.
CDL_TEST_CASE(SDLWindow, ContentScaleWithoutAWindowIsThePrimaryDisplays, Display)
{
	WindowingBackend backend;
	Window window(TestWindowSpec());
	window.Destroy();

	CDL_EXPECT_EQ(window.GetContentScale(), SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay()));
}
