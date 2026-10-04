// The SDL backend's event translation, fed synthetic SDL events. Only SDL's event queue is started,
// never video, so these run headless.

#include "TestFramework.h"
#include "TestHelpers.h"

#include <SDL3/SDL.h>

#include <vector>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Unit;

namespace {

	// Owns SDL's event subsystem for one test; quitting it discards anything still queued.
	struct SDLEventQueue
	{
		SDLEventQueue() { CDL_CHECK_MSG(SDL_InitSubSystem(SDL_INIT_EVENTS), SDL_GetError()); }
		~SDLEventQueue() { SDL_QuitSubSystem(SDL_INIT_EVENTS); }

		SDLEventQueue(const SDLEventQueue&) = delete;
		SDLEventQueue& operator=(const SDLEventQueue&) = delete;
	};

	void PushWindowEvent(SDL_EventType type, SDL_WindowID windowID = 1, int32_t data1 = 0, int32_t data2 = 0)
	{
		SDL_Event e{};
		e.type = type;
		e.window.windowID = windowID;
		e.window.data1 = data1;
		e.window.data2 = data2;
		CDL_CHECK_MSG(SDL_PushEvent(&e), SDL_GetError());
	}

	void PushQuitEvent()
	{
		SDL_Event e{};
		e.type = SDL_EVENT_QUIT;
		CDL_CHECK_MSG(SDL_PushEvent(&e), SDL_GetError());
	}

	std::vector<PlatformEvent> Pump()
	{
		std::vector<PlatformEvent> events;
		Platform::PumpEvents(events);
		return events;
	}

}

CDL_TEST_CASE(SDLEvents, QuitBecomesQuitRequested, Unit)
{
	SDLEventQueue queue;
	PushQuitEvent();

	const std::vector<PlatformEvent> events = Pump();
	CDL_CHECK_EQ(events.size(), 1u);
	CDL_CHECK(std::holds_alternative<QuitRequested>(events[0]));
}

CDL_TEST_CASE(SDLEvents, CloseRequestCarriesTheWindowID, Unit)
{
	SDLEventQueue queue;
	PushWindowEvent(SDL_EVENT_WINDOW_CLOSE_REQUESTED, 42);

	const std::vector<PlatformEvent> events = Pump();
	CDL_CHECK_EQ(events.size(), 1u);
	const auto* close = std::get_if<WindowCloseRequested>(&events[0]);
	CDL_CHECK(close != nullptr);
	CDL_EXPECT_EQ(close->WindowID, 42u);
}

CDL_TEST_CASE(SDLEvents, PixelSizeChangeBecomesResized, Unit)
{
	SDLEventQueue queue;
	PushWindowEvent(SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED, 1, 1280, 720);

	const std::vector<PlatformEvent> events = Pump();
	CDL_CHECK_EQ(events.size(), 1u);
	const auto* resized = std::get_if<WindowResized>(&events[0]);
	CDL_CHECK(resized != nullptr);
	CDL_EXPECT_EQ(resized->Width, 1280u);
	CDL_EXPECT_EQ(resized->Height, 720u);
}

CDL_TEST_CASE(SDLEvents, FocusGainAndLossAreReported, Unit)
{
	SDLEventQueue queue;
	PushWindowEvent(SDL_EVENT_WINDOW_FOCUS_GAINED);
	PushWindowEvent(SDL_EVENT_WINDOW_FOCUS_LOST);

	const std::vector<PlatformEvent> events = Pump();
	CDL_CHECK_EQ(events.size(), 2u);
	CDL_CHECK(std::holds_alternative<WindowFocus>(events[0]) && std::holds_alternative<WindowFocus>(events[1]));
	CDL_EXPECT(std::get<WindowFocus>(events[0]).Focused);
	CDL_EXPECT_FALSE(std::get<WindowFocus>(events[1]).Focused);
}

// Restore is what resumes a renderer paused on minimize, so it must be reported, not just minimize.
CDL_TEST_CASE(SDLEvents, MinimizeAndRestoreSetTheMinimizedState, Unit)
{
	SDLEventQueue queue;
	PushWindowEvent(SDL_EVENT_WINDOW_MINIMIZED);
	PushWindowEvent(SDL_EVENT_WINDOW_RESTORED);

	const std::vector<PlatformEvent> events = Pump();
	CDL_CHECK_EQ(events.size(), 2u);
	CDL_CHECK(std::holds_alternative<WindowMinimized>(events[0]) && std::holds_alternative<WindowMinimized>(events[1]));
	CDL_EXPECT(std::get<WindowMinimized>(events[0]).Minimized);
	CDL_EXPECT_FALSE(std::get<WindowMinimized>(events[1]).Minimized);
}

// RESIZED is in window coordinates; only PIXEL_SIZE_CHANGED is reported, because pixels are what the swapchain needs.
CDL_TEST_CASE(SDLEvents, UnhandledEventsAreDropped, Unit)
{
	SDLEventQueue queue;
	PushWindowEvent(SDL_EVENT_WINDOW_RESIZED, 1, 640, 480);
	PushWindowEvent(SDL_EVENT_WINDOW_MAXIMIZED);
	PushWindowEvent(SDL_EVENT_WINDOW_MOVED, 1, 10, 10);

	CDL_EXPECT(Pump().empty());
}

CDL_TEST_CASE(SDLEvents, MixedEventsKeepArrivalOrder, Unit)
{
	SDLEventQueue queue;
	PushWindowEvent(SDL_EVENT_WINDOW_FOCUS_LOST);
	PushWindowEvent(SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED, 1, 800, 600);
	PushWindowEvent(SDL_EVENT_WINDOW_RESIZED, 1, 800, 600);	// Dropped, and must not leave a gap
	PushQuitEvent();

	const std::vector<PlatformEvent> events = Pump();
	CDL_CHECK_EQ(events.size(), 3u);
	CDL_EXPECT(std::holds_alternative<WindowFocus>(events[0]));
	CDL_EXPECT(std::holds_alternative<WindowResized>(events[1]));
	CDL_EXPECT(std::holds_alternative<QuitRequested>(events[2]));
}

// The caller reuses one buffer every frame: last frame's events must go, but the capacity must stay so pumping doesn't allocate.
CDL_TEST_CASE(SDLEvents, PumpClearsButKeepsTheCallersCapacity, Unit)
{
	SDLEventQueue queue;

	std::vector<PlatformEvent> events;
	events.reserve(64);
	events.emplace_back(QuitRequested{});

	Platform::PumpEvents(events);
	CDL_EXPECT(events.empty());
	CDL_EXPECT_GE(events.capacity(), 64u);
}
