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

	void PushKeyEvent(SDL_EventType type, SDL_Scancode scancode, SDL_Keycode key = SDLK_UNKNOWN, bool repeat = false)
	{
		SDL_Event e{};
		e.type = type;
		e.key.scancode = scancode;
		e.key.key = key;
		e.key.down = type == SDL_EVENT_KEY_DOWN;
		e.key.repeat = repeat;
		CDL_CHECK_MSG(SDL_PushEvent(&e), SDL_GetError());
	}

	void PushMouseButtonEvent(SDL_EventType type, Uint8 button)
	{
		SDL_Event e{};
		e.type = type;
		e.button.button = button;
		e.button.down = type == SDL_EVENT_MOUSE_BUTTON_DOWN;
		CDL_CHECK_MSG(SDL_PushEvent(&e), SDL_GetError());
	}

	void PushMouseMotionEvent(float x, float y, float xrel, float yrel)
	{
		SDL_Event e{};
		e.type = SDL_EVENT_MOUSE_MOTION;
		e.motion.x = x;
		e.motion.y = y;
		e.motion.xrel = xrel;
		e.motion.yrel = yrel;
		CDL_CHECK_MSG(SDL_PushEvent(&e), SDL_GetError());
	}

	void PushMouseWheelEvent(float x, float y, Sint32 integerX, Sint32 integerY)
	{
		SDL_Event e{};
		e.type = SDL_EVENT_MOUSE_WHEEL;
		e.wheel.x = x;
		e.wheel.y = y;
		e.wheel.integer_x = integerX;
		e.wheel.integer_y = integerY;
		CDL_CHECK_MSG(SDL_PushEvent(&e), SDL_GetError());
	}

	std::vector<PlatformEvent> Pump()
	{
		std::vector<PlatformEvent> events;
		Platform::PumpEvents(events);
		return events;
	}

	// Input events sit one level down, inside PlatformEvent's InputEvent alternative.
	template<typename T>
	const T* GetInput(const PlatformEvent& event)
	{
		const InputEvent* input = std::get_if<InputEvent>(&event);
		return input ? std::get_if<T>(input) : nullptr;
	}

	// Pump, then feed the input events to InputSystem the way Application does.
	InputSnapshot PumpInto(InputSystem& input)
	{
		for (const PlatformEvent& event : Pump())
		{
			if (const InputEvent* inputEvent = std::get_if<InputEvent>(&event))
				input.ConsumeEvent(*inputEvent);
		}
		return input.CaptureSnapshot();
	}

}

// The backend turns SDL codes into Candle codes with a cast, which is only correct while the numbers agree.
// Spot checks at each block boundary, so an off-by-one or an SDL renumbering breaks the build.
CDL_STATIC_ASSERT(std::to_underlying(KeyCode::Count) == SDL_SCANCODE_COUNT);
CDL_STATIC_ASSERT(std::to_underlying(KeyCode::A) == SDL_SCANCODE_A);
CDL_STATIC_ASSERT(std::to_underlying(KeyCode::D0) == SDL_SCANCODE_0);
CDL_STATIC_ASSERT(std::to_underlying(KeyCode::Enter) == SDL_SCANCODE_RETURN);
CDL_STATIC_ASSERT(std::to_underlying(KeyCode::F12) == SDL_SCANCODE_F12);
CDL_STATIC_ASSERT(std::to_underlying(KeyCode::NumPadDecimal) == SDL_SCANCODE_KP_PERIOD);
CDL_STATIC_ASSERT(std::to_underlying(KeyCode::Menu) == SDL_SCANCODE_APPLICATION);
CDL_STATIC_ASSERT(std::to_underlying(KeyCode::F24) == SDL_SCANCODE_F24);
CDL_STATIC_ASSERT(std::to_underlying(KeyCode::ExSel) == SDL_SCANCODE_EXSEL);
CDL_STATIC_ASSERT(std::to_underlying(KeyCode::NumPadHexadecimal) == SDL_SCANCODE_KP_HEXADECIMAL);
CDL_STATIC_ASSERT(std::to_underlying(KeyCode::RightSuper) == SDL_SCANCODE_RGUI);
CDL_STATIC_ASSERT(std::to_underlying(KeyCode::EndCall) == SDL_SCANCODE_ENDCALL);

CDL_STATIC_ASSERT(std::to_underlying(MouseButton::Left) == SDL_BUTTON_LEFT);
CDL_STATIC_ASSERT(std::to_underlying(MouseButton::Middle) == SDL_BUTTON_MIDDLE);
CDL_STATIC_ASSERT(std::to_underlying(MouseButton::Right) == SDL_BUTTON_RIGHT);
CDL_STATIC_ASSERT(std::to_underlying(MouseButton::X1) == SDL_BUTTON_X1);
CDL_STATIC_ASSERT(std::to_underlying(MouseButton::X2) == SDL_BUTTON_X2);

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

//////////////////////////////////////////////////////////////////////////
// Input events
//////////////////////////////////////////////////////////////////////////

// On AZERTY the key in QWERTY's W position types Z; gameplay binds the position, so the scancode must win.
CDL_TEST_CASE(SDLEvents, KeyEventsCarryTheScancodeNotTheKeycode, Unit)
{
	SDLEventQueue queue;
	PushKeyEvent(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W, SDLK_Z);
	PushKeyEvent(SDL_EVENT_KEY_UP, SDL_SCANCODE_W, SDLK_Z);

	const std::vector<PlatformEvent> events = Pump();
	CDL_CHECK_EQ(events.size(), 2u);
	const auto* down = GetInput<KeyPressedEvent>(events[0]);
	const auto* up = GetInput<KeyReleasedEvent>(events[1]);
	CDL_CHECK(down != nullptr && up != nullptr);
	CDL_EXPECT_EQ(down->Key, KeyCode::W);
	CDL_EXPECT_EQ(up->Key, KeyCode::W);
}

// Gameplay reads Pressed as "the player just hit this key"; OS auto-repeat must not fake that ~30 times a second.
CDL_TEST_CASE(SDLEvents, KeyRepeatIsNotAFreshPress, Unit)
{
	SDLEventQueue queue;
	InputSystem input;

	PushKeyEvent(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE, SDLK_SPACE);
	CDL_CHECK(PumpInto(input).IsKeyPressed(KeyCode::Space));

	PushKeyEvent(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE, SDLK_SPACE, true);
	const InputSnapshot repeated = PumpInto(input);
	CDL_EXPECT(repeated.IsKeyDown(KeyCode::Space));
	CDL_EXPECT_FALSE(repeated.IsKeyPressed(KeyCode::Space));
}

CDL_TEST_CASE(SDLEvents, MouseButtonsTranslateToTheMatchingButton, Unit)
{
	SDLEventQueue queue;
	PushMouseButtonEvent(SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_BUTTON_RIGHT);
	PushMouseButtonEvent(SDL_EVENT_MOUSE_BUTTON_UP, SDL_BUTTON_X2);

	const std::vector<PlatformEvent> events = Pump();
	CDL_CHECK_EQ(events.size(), 2u);
	const auto* down = GetInput<MouseButtonPressedEvent>(events[0]);
	const auto* up = GetInput<MouseButtonReleasedEvent>(events[1]);
	CDL_CHECK(down != nullptr && up != nullptr);
	CDL_EXPECT_EQ(down->Button, MouseButton::Right);
	CDL_EXPECT_EQ(up->Button, MouseButton::X2);
}

// Windows never reports past X2, but a button with no bit in the snapshot must not reach InputSystem as a real one.
CDL_TEST_CASE(SDLEvents, OutOfRangeMouseButtonsBecomeUnknown, Unit)
{
	SDLEventQueue queue;
	PushMouseButtonEvent(SDL_EVENT_MOUSE_BUTTON_DOWN, 0);
	PushMouseButtonEvent(SDL_EVENT_MOUSE_BUTTON_DOWN, 6);

	const std::vector<PlatformEvent> events = Pump();
	CDL_CHECK_EQ(events.size(), 2u);
	const auto* zero = GetInput<MouseButtonPressedEvent>(events[0]);
	const auto* six = GetInput<MouseButtonPressedEvent>(events[1]);
	CDL_CHECK(zero != nullptr && six != nullptr);
	CDL_EXPECT_EQ(zero->Button, MouseButton::Unknown);
	CDL_EXPECT_EQ(six->Button, MouseButton::Unknown);
}

CDL_TEST_CASE(SDLEvents, MouseMotionCarriesPositionAndRelativeDelta, Unit)
{
	SDLEventQueue queue;
	PushMouseMotionEvent(100.5f, 200.25f, 3.0f, -4.0f);

	const std::vector<PlatformEvent> events = Pump();
	CDL_CHECK_EQ(events.size(), 1u);
	const auto* move = GetInput<MouseMoveEvent>(events[0]);
	CDL_CHECK(move != nullptr);
	CDL_EXPECT_EQ(move->Position.x, 100.5f);
	CDL_EXPECT_EQ(move->Position.y, 200.25f);
	CDL_EXPECT_EQ(move->Delta.x, 3.0f);
	CDL_EXPECT_EQ(move->Delta.y, -4.0f);
}

// A precision touchpad sends fractions, which truncating each event's float never adds up; SDL's integer_* accumulates them.
CDL_TEST_CASE(SDLEvents, WheelTicksComeFromSDLsAccumulatedNotches, Unit)
{
	SDLEventQueue queue;
	PushMouseWheelEvent(-0.25f, 0.6f, 0, 1);	// This event tipped the accumulated vertical scroll past one notch

	const std::vector<PlatformEvent> events = Pump();
	CDL_CHECK_EQ(events.size(), 1u);
	const auto* wheel = GetInput<MouseWheelEvent>(events[0]);
	CDL_CHECK(wheel != nullptr);
	CDL_EXPECT_EQ(wheel->Delta.x, -0.25f);
	CDL_EXPECT_EQ(wheel->Delta.y, 0.6f);
	CDL_EXPECT_EQ(wheel->Ticks.x, 0);
	CDL_EXPECT_EQ(wheel->Ticks.y, 1);
}
