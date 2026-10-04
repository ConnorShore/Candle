// Application driven by real SDL events. Opens a real OS window, so these are the Display type.

#include "TestFramework.h"
#include "TestHelpers.h"

#include <SDL3/SDL.h>

#include <optional>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Display;

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
