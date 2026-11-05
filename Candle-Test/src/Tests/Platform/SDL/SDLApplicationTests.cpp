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

	void PushMouseButtonDown(Uint8 button)
	{
		SDL_Event e{};
		e.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
		e.button.button = button;
		e.button.down = true;
		CDL_CHECK_MSG(SDL_PushEvent(&e), SDL_GetError());
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
			const ImGuiWindow* console = ImGui::FindWindowByName("Debug Console");
			const bool consoleDrawn = console && (console->Active || console->WasActive);

			switch (m_Updates++)
			{
			case 0:
				ClosedAtStart = !consoleDrawn;
				PushKeyDown(SDL_SCANCODE_GRAVE);
				break;
			case 3:
				OpenAfterHotkey = consoleDrawn;
				PushKeyDown(SDL_SCANCODE_F1);
				break;
			case 6:
				HiddenAfterF1 = !consoleDrawn;
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
