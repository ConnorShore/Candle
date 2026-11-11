// The main loop's quit path. Run() has no timeout, so a broken RequestQuit hangs the runner rather
// than failing it; there is no way to stop Run() from outside other than the flag under test.

#include "TestFramework.h"
#include "TestHelpers.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <filesystem>
#include <optional>
#include <thread>
#include <vector>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Unit;
using Candle::Test::Type::Display;

namespace {

	// Keeps what its first update was handed through UpdateContext::Render, then quits.
	class RenderContextProbeApplication : public TestApplication
	{
	public:
		using TestApplication::TestApplication;

		void OnUpdate(UpdateContext context) override
		{
			RenderingToSwapChain = context.Render.IsRenderingToSwapChain();
			InitialOutputSize = context.Render.GetOutputSize();
			HasOutputTarget = context.Render.GetOutputTexture().IsValid();
			RequestQuit();
		}

		bool RenderingToSwapChain{ false };
		glm::uvec2 InitialOutputSize{ 0, 0 };
		bool HasOutputTarget{ true };
	};

	// Records the ImGui frame each update ran inside, drawing a window in each as an application would, and quits after three.
	class GuiFrameApplication : public TestApplication
	{
	public:
		using TestApplication::TestApplication;

		void OnUpdate(UpdateContext context) override
		{
			const ImGuiContext* gui = ImGui::GetCurrentContext();
			Updates.push_back({ .FrameOpen = gui && gui->WithinFrameScope, .FrameCount = gui ? ImGui::GetFrameCount() : -1 });
			if (gui)
			{
				ImGui::Begin("Update");
				ImGui::Text("Update %zu", Updates.size());
				ImGui::End();
			}

			if (Updates.size() == 3)
				RequestQuit();
		}

		struct Update
		{
			bool FrameOpen;
			int FrameCount;
		};
		std::vector<Update> Updates;
	};

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

// Anchored to the executable, so the app finds its assets and what it saved from any working directory; an absolute path is
// used as given.
CDL_TEST_CASE(Application, DirectoriesResolveAgainstTheExecutableUnlessAbsolute, Unit)
{
	const std::filesystem::path exeDir = Platform::GetExecutableDirectory();
	const std::filesystem::path absolute = exeDir.root_path() / "ProjectAssets";
	{
		ApplicationSpecification spec = QuietSpec();
		spec.EngineAssetDir = "EngineAssets";
		spec.ProjectAssetDir = absolute;
		spec.SavedDir = "SavedFiles";

		TestApplication app(spec);
		CDL_EXPECT(app.GetSpecification().EngineAssetDir == exeDir / "EngineAssets");
		CDL_EXPECT(app.GetSpecification().ProjectAssetDir == absolute);
		CDL_EXPECT(app.GetSpecification().SavedDir == exeDir / "SavedFiles");
	}
	RestoreLoggerDefaults();
}

// Premake bakes the default in as an absolute path, so a default-built app never depends on the working directory either.
CDL_TEST_CASE(Application, DefaultEngineAssetDirIsAbsolute, Unit)
{
	CDL_EXPECT(ApplicationSpecification{}.EngineAssetDir.is_absolute());
}

// The spec's choice reaches the app through UpdateContext::Render, sized from the window spec; headless there is no
// RenderManager, so no output target either way.
CDL_TEST_CASE(Application, HeadlessUpdateGetsTheRenderContextTheSpecAsksFor, Unit)
{
	for (const bool renderToSwapChain : { true, false })
	{
		ApplicationSpecification spec = QuietSpec();
		spec.RenderSpec.RenderToSwapChain = renderToSwapChain;
		spec.WindowSpec = TestWindowSpec(320, 200);

		RenderContextProbeApplication app(spec);
		app.Run();
		CDL_EXPECT_EQ(app.RenderingToSwapChain, renderToSwapChain);
		CDL_EXPECT(app.InitialOutputSize == glm::uvec2(320, 200));
		CDL_EXPECT_FALSE(app.HasOutputTarget);
	}
	RestoreLoggerDefaults();
}

// The GUI frame brackets OnUpdate, so an app can make ImGui calls there: a frame is open on entry, and each update is
// inside the next one.
CDL_TEST_CASE(Application, WindowedUpdatesRunInsideAGuiFrame, Display)
{
	std::vector<GuiFrameApplication::Update> updates;
	{
		std::optional<GuiFrameApplication> app;
		EmplaceWindowedApp(app);
		app->Run();
		updates = app->Updates;
	}
	RestoreLoggerDefaults();

	CDL_CHECK_EQ(updates.size(), 3u);
	for (size_t i = 0; i < updates.size(); ++i)
	{
		CDL_EXPECT_MSG(updates[i].FrameOpen, std::format("update {} ran outside an ImGui frame", i));
		if (i > 0)
			CDL_EXPECT_MSG(updates[i].FrameCount == updates[i - 1].FrameCount + 1, std::format("update {} is in ImGui frame {}, after {}", i, updates[i].FrameCount, updates[i - 1].FrameCount));
	}
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
