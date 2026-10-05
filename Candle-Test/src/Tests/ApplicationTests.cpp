// The main loop's quit path. Run() has no timeout, so a broken RequestQuit hangs the runner rather
// than failing it; there is no way to stop Run() from outside other than the flag under test.

#include "TestFramework.h"
#include "TestHelpers.h"

#include <optional>
#include <thread>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Unit;
using Candle::Test::Type::Display;

CDL_TEST_CASE(Application, RunReturnsImmediatelyIfQuitAlreadyRequested, Unit)
{
	{
		TestApplication app(QuietSpec());
		app.RequestQuit();

		const Timer timer;
		app.Run();
		CDL_EXPECT_LT(timer.ElapsedMilliseconds(), 100.0);
	}
	RestoreLoggerDefaults();
}

CDL_TEST_CASE(Application, RequestQuitIsIdempotent, Unit)
{
	{
		TestApplication app(QuietSpec());
		app.RequestQuit();
		app.RequestQuit();
		app.Run();
	}
	RestoreLoggerDefaults();
}

CDL_TEST_CASE(Application, RequestQuitFromAnotherThreadStopsRun, Unit)
{
	double runMs = 0.0;
	{
		TestApplication app(QuietSpec());

		const Timer timer;
		std::jthread quitter([&app] {
			Platform::SleepCurrentThread(20);
			app.RequestQuit();
		});

		app.Run();
		runMs = timer.ElapsedMilliseconds();
	}
	RestoreLoggerDefaults();

	CDL_NOTE(std::format("Run returned after {:.3f} ms", runMs));

	// Run cannot return before the flag is set, and the quitter sleeps at least 20 ms first.
	CDL_EXPECT_GE(runMs, 20.0);
	CDL_EXPECT_LT(runMs, 1000.0);
}

// Headless is what keeps this suite runnable without a display, so it must never start the windowing backend.
CDL_TEST_CASE(Application, HeadlessNeverStartsWindowing, Unit)
{
	CDL_CHECK_FALSE(Platform::IsWindowingInitialized());
	{
		TestApplication app(QuietSpec());
		CDL_EXPECT_FALSE(Platform::IsWindowingInitialized());
	}
	RestoreLoggerDefaults();
}

// The windowing backend must be up for the app's whole lifetime and fully shut down once it is destroyed.
CDL_TEST_CASE(Application, WindowedAppOwnsTheWindowingBackend, Display)
{
	{
		std::optional<TestApplication> app;
		EmplaceWindowedApp(app);
		CDL_EXPECT(Platform::IsWindowingInitialized());
	}
	RestoreLoggerDefaults();

	CDL_EXPECT_FALSE(Platform::IsWindowingInitialized());
}
