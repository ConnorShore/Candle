// InputSystem fed PlatformEvents directly, with no backend and no window, so every test runs headless.
// How SDL events become PlatformEvents is tested in Tests/Platform/SDL/SDLEventTests.cpp.

#include "TestFramework.h"
#include "TestHelpers.h"

#include <initializer_list>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Unit;

namespace {

	// One frame: consume its events, then capture. Returns by value, as Application does.
	InputSnapshot Frame(InputSystem& input, std::initializer_list<PlatformEvent> events = {})
	{
		for (const PlatformEvent& event : events)
			input.ConsumeEvent(event);
		return input.CaptureSnapshot();
	}

	constexpr KeyCode LastScancode = static_cast<KeyCode>(std::to_underlying(KeyCode::Count) - 1);

	constexpr MouseButton AllButtons[] = { MouseButton::Left, MouseButton::Middle, MouseButton::Right, MouseButton::X1, MouseButton::X2 };

}

CDL_TEST_CASE(InputSystem, FirstSnapshotIsEmpty, Unit)
{
	InputSystem input;
	const InputSnapshot snapshot = Frame(input);

	CDL_EXPECT(snapshot.KeysDown.none());
	CDL_EXPECT(snapshot.KeysPressed.none());
	CDL_EXPECT(snapshot.KeysReleased.none());
	CDL_EXPECT_EQ(snapshot.MouseDown, 0);
	CDL_EXPECT_EQ(snapshot.MousePressed, 0);
	CDL_EXPECT_EQ(snapshot.MouseReleased, 0);
	CDL_EXPECT(snapshot.MousePosition == glm::vec2(0.0f));
	CDL_EXPECT(snapshot.MouseDelta == glm::vec2(0.0f));
	CDL_EXPECT(snapshot.WheelDelta == glm::vec2(0.0f));
	CDL_EXPECT(snapshot.WheelTicks == glm::ivec2(0));
}

//////////////////////////////////////////////////////////////////////////
// Keys
//////////////////////////////////////////////////////////////////////////

CDL_TEST_CASE(InputSystem, KeyPressReportsDownAndPressed, Unit)
{
	InputSystem input;
	const InputSnapshot snapshot = Frame(input, { KeyPressedEvent{ KeyCode::Space } });

	CDL_EXPECT(snapshot.IsKeyDown(KeyCode::Space));
	CDL_EXPECT(snapshot.IsKeyPressed(KeyCode::Space));
	CDL_EXPECT_FALSE(snapshot.IsKeyReleased(KeyCode::Space));
	CDL_EXPECT_EQ(snapshot.KeysDown.count(), 1u);
}

// Levels carry across frames; edges belong to the one frame they happened in.
CDL_TEST_CASE(InputSystem, HeldKeyStaysDownButIsPressedOnlyOnce, Unit)
{
	InputSystem input;
	Frame(input, { KeyPressedEvent{ KeyCode::W } });
	const InputSnapshot held = Frame(input);

	CDL_EXPECT(held.IsKeyDown(KeyCode::W));
	CDL_EXPECT_FALSE(held.IsKeyPressed(KeyCode::W));
	CDL_EXPECT_FALSE(held.IsKeyReleased(KeyCode::W));
}

CDL_TEST_CASE(InputSystem, KeyReleaseReportsReleasedAndClearsDown, Unit)
{
	InputSystem input;
	Frame(input, { KeyPressedEvent{ KeyCode::W } });
	const InputSnapshot released = Frame(input, { KeyReleasedEvent{ KeyCode::W } });

	CDL_EXPECT_FALSE(released.IsKeyDown(KeyCode::W));
	CDL_EXPECT_FALSE(released.IsKeyPressed(KeyCode::W));
	CDL_EXPECT(released.IsKeyReleased(KeyCode::W));

	CDL_EXPECT_FALSE(Frame(input).IsKeyReleased(KeyCode::W));
}

// Why Pressed is latched rather than derived from two frames' Down levels: a tap shorter than a frame would vanish.
CDL_TEST_CASE(InputSystem, TapWithinOneFrameReportsBothEdges, Unit)
{
	InputSystem input;
	const InputSnapshot tapped = Frame(input, { KeyPressedEvent{ KeyCode::Space }, KeyReleasedEvent{ KeyCode::Space } });

	CDL_EXPECT_FALSE(tapped.IsKeyDown(KeyCode::Space));
	CDL_EXPECT(tapped.IsKeyPressed(KeyCode::Space));
	CDL_EXPECT(tapped.IsKeyReleased(KeyCode::Space));

	const InputSnapshot after = Frame(input);
	CDL_EXPECT_FALSE(after.IsKeyPressed(KeyCode::Space));
	CDL_EXPECT_FALSE(after.IsKeyReleased(KeyCode::Space));
}

CDL_TEST_CASE(InputSystem, ReleaseAndRepressWithinOneFrameEndsDown, Unit)
{
	InputSystem input;
	Frame(input, { KeyPressedEvent{ KeyCode::Space } });
	const InputSnapshot snapshot = Frame(input, { KeyReleasedEvent{ KeyCode::Space }, KeyPressedEvent{ KeyCode::Space } });

	CDL_EXPECT(snapshot.IsKeyDown(KeyCode::Space));
	CDL_EXPECT(snapshot.IsKeyPressed(KeyCode::Space));
	CDL_EXPECT(snapshot.IsKeyReleased(KeyCode::Space));
}

// The lowest named key, the highest named key and the bitset's last bit.
CDL_TEST_CASE(InputSystem, KeysAcrossTheWholeRangeAreIndependent, Unit)
{
	InputSystem input;
	const InputSnapshot snapshot = Frame(input, {
		KeyPressedEvent{ KeyCode::A }, KeyPressedEvent{ KeyCode::EndCall }, KeyPressedEvent{ LastScancode }, KeyReleasedEvent{ KeyCode::EndCall } });

	CDL_EXPECT(snapshot.IsKeyDown(KeyCode::A));
	CDL_EXPECT(snapshot.IsKeyDown(LastScancode));
	CDL_EXPECT_FALSE(snapshot.IsKeyDown(KeyCode::EndCall));
	CDL_EXPECT_EQ(snapshot.KeysDown.count(), 2u);
}

// Every key SDL can't identify arrives as Unknown, so a held Unknown level would stand for several physical keys.
CDL_TEST_CASE(InputSystem, UnknownKeyIsIgnored, Unit)
{
	InputSystem input;
	const InputSnapshot snapshot = Frame(input, { KeyPressedEvent{ KeyCode::Unknown } });

	CDL_EXPECT(snapshot.KeysDown.none());
	CDL_EXPECT(snapshot.KeysPressed.none());
}

//////////////////////////////////////////////////////////////////////////
// Mouse buttons
//////////////////////////////////////////////////////////////////////////

// The snapshot promises SDL_BUTTON_MASK's layout, bit (button - 1), so the raw masks are part of its contract.
CDL_TEST_CASE(InputSystem, EachMouseButtonOwnsOneBit, Unit)
{
	for (MouseButton button : AllButtons)
	{
		InputSystem input;
		const InputSnapshot snapshot = Frame(input, { MouseButtonPressedEvent{ button } });
		const int expectedMask = 1 << (std::to_underlying(button) - 1);

		CDL_EXPECT_EQ(snapshot.MouseDown, expectedMask);
		CDL_EXPECT_EQ(snapshot.MousePressed, expectedMask);
		for (MouseButton other : AllButtons)
			CDL_EXPECT_EQ(snapshot.IsMouseDown(other), other == button);
	}
}

CDL_TEST_CASE(InputSystem, HeldMouseButtonStaysDownButIsPressedOnlyOnce, Unit)
{
	InputSystem input;
	Frame(input, { MouseButtonPressedEvent{ MouseButton::Right } });
	const InputSnapshot held = Frame(input);

	CDL_EXPECT(held.IsMouseDown(MouseButton::Right));
	CDL_EXPECT_FALSE(held.IsMousePressed(MouseButton::Right));

	const InputSnapshot released = Frame(input, { MouseButtonReleasedEvent{ MouseButton::Right } });
	CDL_EXPECT_FALSE(released.IsMouseDown(MouseButton::Right));
	CDL_EXPECT(released.IsMouseReleased(MouseButton::Right));
}

CDL_TEST_CASE(InputSystem, ClickWithinOneFrameReportsBothEdges, Unit)
{
	InputSystem input;
	const InputSnapshot clicked = Frame(input, { MouseButtonPressedEvent{ MouseButton::Left }, MouseButtonReleasedEvent{ MouseButton::Left } });

	CDL_EXPECT_FALSE(clicked.IsMouseDown(MouseButton::Left));
	CDL_EXPECT(clicked.IsMousePressed(MouseButton::Left));
	CDL_EXPECT(clicked.IsMouseReleased(MouseButton::Left));
}

// Unknown is 0, so the (button - 1) shift must never be taken for it.
CDL_TEST_CASE(InputSystem, UnknownMouseButtonIsIgnored, Unit)
{
	InputSystem input;
	const InputSnapshot snapshot = Frame(input, { MouseButtonPressedEvent{ MouseButton::Unknown } });

	CDL_EXPECT_EQ(snapshot.MouseDown, 0);
	CDL_EXPECT_EQ(snapshot.MousePressed, 0);
	CDL_EXPECT_FALSE(snapshot.IsMouseDown(MouseButton::Unknown));
}

//////////////////////////////////////////////////////////////////////////
// Mouse motion and wheel
//////////////////////////////////////////////////////////////////////////

// A 1000 Hz mouse sends about 16 motion events per 60 Hz frame; the frame's delta is all of them, not the last.
CDL_TEST_CASE(InputSystem, MouseMotionSumsDeltasAndKeepsTheLatestPosition, Unit)
{
	InputSystem input;
	const InputSnapshot snapshot = Frame(input, {
		MouseMoveEvent{ .Position = { 10.0f, 20.0f }, .Delta = { 3.0f, 4.0f } },
		MouseMoveEvent{ .Position = { 11.0f, 18.0f }, .Delta = { 1.0f, -2.0f } } });

	CDL_EXPECT_EQ(snapshot.MouseDelta.x, 4.0f);
	CDL_EXPECT_EQ(snapshot.MouseDelta.y, 2.0f);
	CDL_EXPECT_EQ(snapshot.MousePosition.x, 11.0f);
	CDL_EXPECT_EQ(snapshot.MousePosition.y, 18.0f);
}

// Position is a level and survives the frame; delta is motion within the frame and doesn't.
CDL_TEST_CASE(InputSystem, MouseDeltaResetsButPositionPersists, Unit)
{
	InputSystem input;
	Frame(input, { MouseMoveEvent{ .Position = { 5.0f, 6.0f }, .Delta = { 1.0f, 1.0f } } });
	const InputSnapshot still = Frame(input);

	CDL_EXPECT(still.MouseDelta == glm::vec2(0.0f));
	CDL_EXPECT_EQ(still.MousePosition.x, 5.0f);
	CDL_EXPECT_EQ(still.MousePosition.y, 6.0f);
}

// Fast flicks and precision touchpads send several wheel events per frame.
CDL_TEST_CASE(InputSystem, WheelSumsDeltasAndTicksWithinAFrame, Unit)
{
	InputSystem input;
	const InputSnapshot snapshot = Frame(input, {
		MouseWheelEvent{ .Delta = { 0.0f, 1.0f }, .Ticks = { 0, 1 } },
		MouseWheelEvent{ .Delta = { 0.25f, 2.0f }, .Ticks = { 0, 2 } } });

	CDL_EXPECT_EQ(snapshot.WheelDelta.x, 0.25f);
	CDL_EXPECT_EQ(snapshot.WheelDelta.y, 3.0f);
	CDL_EXPECT_EQ(snapshot.WheelTicks.x, 0);
	CDL_EXPECT_EQ(snapshot.WheelTicks.y, 3);
}

CDL_TEST_CASE(InputSystem, WheelResetsEachFrame, Unit)
{
	InputSystem input;
	Frame(input, { MouseWheelEvent{ .Delta = { 0.0f, 1.0f }, .Ticks = { 0, 1 } } });
	const InputSnapshot after = Frame(input);

	CDL_EXPECT(after.WheelDelta == glm::vec2(0.0f));
	CDL_EXPECT(after.WheelTicks == glm::ivec2(0));
}

//////////////////////////////////////////////////////////////////////////
// Event routing and publishing
//////////////////////////////////////////////////////////////////////////

// Focus is left out on purpose: clearing held state on focus loss would be a legitimate reaction to it.
CDL_TEST_CASE(InputSystem, WindowEventsDoNotTouchInputState, Unit)
{
	InputSystem input;
	const InputSnapshot snapshot = Frame(input, {
		QuitRequested{}, WindowCloseRequested{ .WindowID = 1 }, WindowResized{ .WindowID = 1, .Width = 800, .Height = 600 },
		WindowMinimized{ .WindowID = 1, .Minimized = true } });

	CDL_EXPECT(snapshot.KeysDown.none());
	CDL_EXPECT(snapshot.KeysPressed.none());
	CDL_EXPECT_EQ(snapshot.MouseDown, 0);
	CDL_EXPECT(snapshot.MouseDelta == glm::vec2(0.0f));
	CDL_EXPECT(snapshot.WheelDelta == glm::vec2(0.0f));
}

// The simulation reads frame N's snapshot while the main thread is already consuming frame N+1's events.
CDL_TEST_CASE(InputSystem, EventsAfterCaptureLeaveThePublishedSnapshotAlone, Unit)
{
	InputSystem input;
	input.ConsumeEvent(KeyPressedEvent{ KeyCode::A });
	const InputSnapshot& published = input.CaptureSnapshot();

	input.ConsumeEvent(KeyPressedEvent{ KeyCode::W });
	input.ConsumeEvent(MouseMoveEvent{ .Position = { 1.0f, 1.0f }, .Delta = { 1.0f, 1.0f } });

	CDL_EXPECT(published.IsKeyPressed(KeyCode::A));
	CDL_EXPECT_FALSE(published.IsKeyDown(KeyCode::W));
	CDL_EXPECT(published.MouseDelta == glm::vec2(0.0f));
}
