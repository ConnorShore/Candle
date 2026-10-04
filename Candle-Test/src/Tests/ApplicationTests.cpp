// The main loop's quit path. Run() has no timeout, so a broken RequestQuit hangs the runner rather
// than failing it; there is no way to stop Run() from outside other than the flag under test.

#include "TestFramework.h"
#include "TestHelpers.h"

#include <thread>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Unit;

namespace {

	class TestApplication : public Application
	{
	public:
		using Application::Application;
		void OnInit() override {}
		void OnShutdown() override {}
	};

	// Application owns a Logger Init/Shutdown cycle; keep it silent and off disk.
	ApplicationSpecification QuietSpec()
	{
		ApplicationSpecification spec;
		spec.Name = "ApplicationTests";
		spec.LoggerSpec.Level = LogLevel::Warn;
		spec.LoggerSpec.LogToConsole = false;
		spec.LoggerSpec.LogToFile = false;
		return spec;
	}

	// Level and mask are process-wide statics that Application sets and never restores.
	void RestoreLoggerDefaults()
	{
		Logger::SetLevel(LogLevel::Info);
		Logger::SetChannelMask(0xFFFF);
	}

}

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
