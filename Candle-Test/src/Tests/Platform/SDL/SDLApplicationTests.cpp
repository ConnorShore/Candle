// Application driven by real SDL events. Opens a real OS window, so these are the Display type.

#include "TestFramework.h"
#include "TestHelpers.h"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <optional>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Display;

namespace {

	void PushKeyDown(SDL_Scancode scancode)
	{
		SDL_Event e{};
		e.type = SDL_EVENT_KEY_DOWN;
		e.key.scancode = scancode;
		e.key.down = true;
		CDL_CHECK_MSG(SDL_PushEvent(&e), SDL_GetError());
	}

	void PushKeyUp(SDL_Scancode scancode)
	{
		SDL_Event e{};
		e.type = SDL_EVENT_KEY_UP;
		e.key.scancode = scancode;
		CDL_CHECK_MSG(SDL_PushEvent(&e), SDL_GetError());
	}

	void PushMouseButtonDown(Uint8 button)
	{
		SDL_Event e{};
		e.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
		e.button.button = button;
		e.button.down = true;
		CDL_CHECK_MSG(SDL_PushEvent(&e), SDL_GetError());
	}

	bool IsConsoleDrawn()
	{
		const ImGuiWindow* console = ImGui::FindWindowByName("Debug Console");
		return console && (console->Active || console->WasActive);
	}

	// Each update sets ImGui's capture flags by hand and pushes a key and a click, which the next update's events carry.
	// StartFrame recomputes the flags, but only after the next update's events are handled, so these decide for exactly them.
	class GuiCaptureApplication : public TestApplication
	{
	public:
		using TestApplication::TestApplication;

		void OnUpdate(UpdateContext context) override
		{
			ImGuiIO& io = ImGui::GetIO();
			switch (m_Updates++)
			{
			case 0:
				io.WantCaptureMouse = true;
				io.WantCaptureKeyboard = false;
				PushKeyDown(SDL_SCANCODE_A);
				PushMouseButtonDown(SDL_BUTTON_LEFT);
				break;
			case 1:
				WhileMouseCaptured = { context.Input.IsKeyPressed(KeyCode::A), context.Input.IsMousePressed(MouseButton::Left) };
				io.WantCaptureMouse = false;
				io.WantCaptureKeyboard = true;
				PushKeyDown(SDL_SCANCODE_B);
				PushMouseButtonDown(SDL_BUTTON_RIGHT);
				break;
			default:
				WhileKeyboardCaptured = { context.Input.IsKeyPressed(KeyCode::B), context.Input.IsMousePressed(MouseButton::Right) };
				RequestQuit();
				break;
			}
		}

		struct Seen
		{
			bool Key{ false };
			bool Click{ false };
		};
		Seen WhileMouseCaptured, WhileKeyboardCaptured;

	private:
		int m_Updates{ 0 };
	};

	// A key takes a frame to reach the panels. Whichever side of NewFrame OnUpdate runs, either Active or WasActive covers the
	// last frame, so each step leaves enough frames for both to have caught up.
	class DebugHotkeyApplication : public TestApplication
	{
	public:
		using TestApplication::TestApplication;

		void OnUpdate(UpdateContext context) override
		{
			switch (m_Updates++)
			{
			case 0:
				ClosedAtStart = !IsConsoleDrawn();
				PushKeyDown(SDL_SCANCODE_GRAVE);
				break;
			case 3:
				OpenAfterHotkey = IsConsoleDrawn();
				PushKeyDown(SDL_SCANCODE_F1);
				break;
			case 6:
				HiddenAfterF1 = !IsConsoleDrawn();
				RequestQuit();
				break;
			default:
				break;
			}
		}

		bool ClosedAtStart{ false };
		bool OpenAfterHotkey{ false };
		bool HiddenAfterF1{ false };

	private:
		int m_Updates{ 0 };
	};

	// The hotkey arrives while ImGui holds the keyboard, which is the state the console's own input box leaves it in
	class CapturedHotkeyApplication : public TestApplication
	{
	public:
		using TestApplication::TestApplication;

		void OnUpdate(UpdateContext context) override
		{
			switch (m_Updates++)
			{
			case 0:
				ImGui::GetIO().WantCaptureKeyboard = true;
				PushKeyDown(SDL_SCANCODE_GRAVE);
				break;
			case 1:
				GameSawHotkey = context.Input.IsKeyPressed(KeyCode::GraveAccent);
				break;
			case 3:
				ConsoleOpened = IsConsoleDrawn();
				RequestQuit();
				break;
			default:
				break;
			}
		}

		bool GameSawHotkey{ true };
		bool ConsoleOpened{ false };

	private:
		int m_Updates{ 0 };
	};

	// Presses a key the game sees, then lets ImGui take the keyboard before the release arrives
	class CapturedReleaseApplication : public TestApplication
	{
	public:
		using TestApplication::TestApplication;

		void OnUpdate(UpdateContext context) override
		{
			switch (m_Updates++)
			{
			case 0:
				PushKeyDown(SDL_SCANCODE_A);
				break;
			case 1:
				HeldBeforeCapture = context.Input.IsKeyDown(KeyCode::A);
				ImGui::GetIO().WantCaptureKeyboard = true;
				PushKeyUp(SDL_SCANCODE_A);
				break;
			default:
				HeldAfterRelease = context.Input.IsKeyDown(KeyCode::A);
				RequestQuit();
				break;
			}
		}

		bool HeldBeforeCapture{ false };
		bool HeldAfterRelease{ true };

	private:
		int m_Updates{ 0 };
	};

}

// End to end: an OS quit travels SDL -> PumpEvents -> HandleEvents -> RequestQuit and ends Run.
CDL_TEST_CASE(SDLApplication, QuitEventFromTheOSStopsRun, Display)
{
	double runMs = 0.0;
	{
		std::optional<TestApplication> app;
		EmplaceWindowedApp(app);

		SDL_Event quit{};
		quit.type = SDL_EVENT_QUIT;
		const bool pushed = SDL_PushEvent(&quit);
		CDL_EXPECT_MSG(pushed, SDL_GetError());

		// Without the event, Run would never return, so only run it if the push landed.
		if (pushed)
		{
			const Timer timer;
			app->Run();
			runMs = timer.ElapsedMilliseconds();
		}
	}
	RestoreLoggerDefaults();

	CDL_EXPECT_LT(runMs, 1000.0);
}

// Application drops the input the GUI wants before InputSystem sees it, so a click on a window never reaches the game,
// while the device the GUI doesn't want still does.
CDL_TEST_CASE(SDLApplication, InputTheGuiCapturesNeverReachesTheGame, Display)
{
	GuiCaptureApplication::Seen whileMouseCaptured, whileKeyboardCaptured;
	{
		std::optional<GuiCaptureApplication> app;
		EmplaceWindowedApp(app);
		app->Run();
		whileMouseCaptured = app->WhileMouseCaptured;
		whileKeyboardCaptured = app->WhileKeyboardCaptured;
	}
	RestoreLoggerDefaults();

	CDL_EXPECT(whileMouseCaptured.Key);
	CDL_EXPECT_FALSE(whileMouseCaptured.Click);
	CDL_EXPECT_FALSE(whileKeyboardCaptured.Key);
	CDL_EXPECT(whileKeyboardCaptured.Click);
}

// Dropping the release too would leave the key down for the game forever
CDL_TEST_CASE(SDLApplication, ReleasesReachTheGameWhileTheGuiCaptures, Display)
{
	bool heldBeforeCapture = false, heldAfterRelease = true;
	{
		std::optional<CapturedReleaseApplication> app;
		EmplaceWindowedApp(app);
		app->Run();
		heldBeforeCapture = app->HeldBeforeCapture;
		heldAfterRelease = app->HeldAfterRelease;
	}
	RestoreLoggerDefaults();

	CDL_EXPECT(heldBeforeCapture);
	CDL_EXPECT_FALSE(heldAfterRelease);
}

// The debug UI's snapshot skips the gate the game's goes through, or the console could never be closed from its own input box
CDL_TEST_CASE(SDLApplication, DebugHotkeysWorkWhileTheGuiCaptures, Display)
{
	bool gameSawHotkey = true, consoleOpened = false;
	{
		std::optional<CapturedHotkeyApplication> app;
		EmplaceWindowedApp(app);
		app->Run();
		gameSawHotkey = app->GameSawHotkey;
		consoleOpened = app->ConsoleOpened;
	}
	RestoreLoggerDefaults();

	CDL_EXPECT_FALSE(gameSawHotkey);
	CDL_EXPECT(consoleOpened);
}

// End to end through both routes a debug key can take: the console's hotkey arrives in the DebugContext's snapshot, while F1
// goes straight from HandlePlatformEvents to DebugUIManager, so it has to hide what's open before that frame's GUI starts.
CDL_TEST_CASE(SDLApplication, DebugHotkeysOpenTheConsoleAndHideTheUI, Display)
{
	bool closedAtStart = false, openAfterHotkey = false, hiddenAfterF1 = false;
	{
		std::optional<DebugHotkeyApplication> app;
		EmplaceWindowedApp(app);
		app->Run();

		closedAtStart = app->ClosedAtStart;
		openAfterHotkey = app->OpenAfterHotkey;
		hiddenAfterF1 = app->HiddenAfterF1;
	}
	RestoreLoggerDefaults();

	CDL_EXPECT(closedAtStart);
	CDL_EXPECT(openAfterHotkey);
	CDL_EXPECT(hiddenAfterF1);
}
