// RenderContext, the application's handle on rendering: the output target it keeps for an editor's scene window, at least
// a pixel in size and replaced on resize, and none when headless or drawing straight to the swapchain. The GPU tests fail
// on any validation message, and the last runs a whole windowed Application drawing its scene into a GUI window.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "RenderTestHelpers.h"

#include <Candle/Core/Job/JobSystem.h>
#include <Candle/Render/GUI/GuiTextureID.h>
#include <Candle/Render/RenderContext.h>
#include <Candle/Render/RenderManager.h>

#include <imgui.h>

#include <array>
#include <format>
#include <optional>
#include <vector>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Unit;
using Candle::Test::Type::Gpu;
using Candle::Test::Type::Display;

namespace {

	constexpr glm::uvec2 kInitialSize{ 640, 480 };

	// A RenderManager whose render thread never starts: RenderContext only creates and releases textures through it.
	struct TestOutputManager
	{
		WindowingBackend Backend;
		Window AppWindow{ TestWindowSpec() };
		JobSystem Jobs;	// Never started; RenderManager only reads its worker count
		RenderManager Manager{ OutputTargetRenderSpec(), ApplicationInfo{ .Name = "Candle-Test" }, kEngineAssetDir, AppWindow, Jobs };

		TexturePool& Pool() { return Manager.GetTexturePool(); }
	};

	bool SameHandle(GPUTextureHandle a, GPUTextureHandle b) { return a.Index == b.Index && a.Generation == b.Generation; }

	// What the Sandbox does with its "Scene" window, at a new size most frames: the output target and depth are replaced
	// while frames drawing the old ones are in flight, a collapsed window asks for 0x0, and an unchanged size keeps its target.
	class SceneWindowApplication : public TestApplication
	{
	public:
		using TestApplication::TestApplication;

		inline static const std::array<glm::vec2, 6> kRequestedSizes = { {
			{ 320.0f, 240.0f }, { 320.0f, 240.0f }, { 200.0f, 300.0f }, { 0.0f, 0.0f }, { 64.5f, 32.5f }, { 400.0f, 120.0f } } };

		// What each update's RenderContext held once the size was set, which is what that frame's packet carries
		struct Frame
		{
			glm::uvec2 Size;
			GPUTextureHandle Target;
		};
		std::vector<Frame> Frames;

		void OnUpdate(UpdateContext context) override
		{
			const glm::vec2 requested = kRequestedSizes[Frames.size()];
			context.Render.SetOutputSize(requested);

			ImGui::Begin("Scene");
			ImGui::Image(context.Render.GetOutputTextureID(), ImVec2(requested.x, requested.y));
			ImGui::End();

			Frames.push_back({ context.Render.GetOutputSize(), context.Render.GetOutputTexture() });
			if (Frames.size() == kRequestedSizes.size())
				RequestQuit();
		}
	};

}

//////////////////////////////////////////////////////////////////////////
// Sizes, with no RenderManager, as when headless
//////////////////////////////////////////////////////////////////////////

// Headless there is no RenderManager to make a target with, whatever the spec asks for.
CDL_TEST_CASE(RenderContext, HeadlessHasNoOutputTarget, Unit)
{
	const RenderContext context(nullptr, false, kInitialSize);

	CDL_EXPECT_FALSE(context.GetOutputTexture().IsValid());
	CDL_EXPECT_EQ(context.GetOutputTextureID(), ToGuiTextureID(GPUTextureHandle{}));
	CDL_EXPECT_FALSE(context.IsRenderingToSwapChain());
	CDL_EXPECT(context.GetOutputSize() == kInitialSize);
}

// A collapsed panel reports a content region of zero or less, and a target needs an area, so each axis clamps to a pixel.
CDL_TEST_CASE(RenderContext, OutputSizeIsAtLeastOnePixel, Unit)
{
	RenderContext context(nullptr, false, { 0, 0 });
	CDL_EXPECT(context.GetOutputSize() == glm::uvec2(1, 1));

	context.SetOutputSize({ -5.0f, 300.0f });
	CDL_EXPECT(context.GetOutputSize() == glm::uvec2(1, 300));

	context.SetOutputSize({ 200.0f, 0.0f });
	CDL_EXPECT(context.GetOutputSize() == glm::uvec2(200, 1));
}

// ImGui's content regions are fractional; a partial pixel is dropped rather than rounded up past the space available.
CDL_TEST_CASE(RenderContext, OutputSizeDropsPartialPixels, Unit)
{
	RenderContext context(nullptr, false, kInitialSize);
	context.SetOutputSize({ 100.75f, 50.25f });
	CDL_EXPECT(context.GetOutputSize() == glm::uvec2(100, 50));
}

//////////////////////////////////////////////////////////////////////////
// The output target, through a real RenderManager
//////////////////////////////////////////////////////////////////////////

// Made up front, so the first packet already has a target, and one the scene can draw into as well as the GUI sample.
CDL_TEST_CASE(RenderContext, CreatesAnOutputTargetOfTheInitialSize, Gpu)
{
	SkipUnlessRendererShadersCompiled();

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestOutputManager test;
		const RenderContext context(&test.Manager, false, kInitialSize);
		const GPUTextureHandle target = context.GetOutputTexture();
		CDL_CHECK(target.IsValid());

		const TextureInfo info = test.Pool().GetTextureInfo(target);
		CDL_EXPECT(test.Pool().IsAlive(target));
		CDL_EXPECT(info.Usage & vk::ImageUsageFlagBits::eColorAttachment);
		CDL_EXPECT(info.Extent == kInitialSize);
		CDL_EXPECT_EQ(context.GetOutputTextureID(), ToGuiTextureID(target));	// What the application hands ImGui::Image
	}
	ExpectNoVulkanDebugMessages(logs);
}

// A target with no area can't be created, so a zero initial size still gets a pixel.
CDL_TEST_CASE(RenderContext, ZeroInitialSizeGetsAOnePixelTarget, Gpu)
{
	SkipUnlessRendererShadersCompiled();

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestOutputManager test;
		const RenderContext context(&test.Manager, false, { 0, 0 });
		CDL_CHECK(context.GetOutputTexture().IsValid());
		CDL_EXPECT(test.Pool().GetTextureInfo(context.GetOutputTexture()).Extent == glm::uvec2(1, 1));
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Released and replaced rather than resized in place, since frames in flight may still sample the old one.
CDL_TEST_CASE(RenderContext, NewSizeReplacesTheOutputTarget, Gpu)
{
	SkipUnlessRendererShadersCompiled();

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestOutputManager test;
		RenderContext context(&test.Manager, false, kInitialSize);
		const GPUTextureHandle before = context.GetOutputTexture();

		context.SetOutputSize({ 300.0f, 200.0f });
		const GPUTextureHandle after = context.GetOutputTexture();

		CDL_EXPECT_FALSE(SameHandle(before, after));
		CDL_EXPECT_FALSE(test.Pool().IsAlive(before));
		CDL_EXPECT(test.Pool().IsAlive(after));
		CDL_EXPECT(test.Pool().GetTextureInfo(after).Extent == glm::uvec2(300, 200));
		CDL_EXPECT(context.GetOutputSize() == glm::uvec2(300, 200));
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The application sets the size every frame, so one that is unchanged once fractions are dropped keeps the target.
CDL_TEST_CASE(RenderContext, UnchangedSizeKeepsTheOutputTarget, Gpu)
{
	SkipUnlessRendererShadersCompiled();

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestOutputManager test;
		RenderContext context(&test.Manager, false, kInitialSize);
		const GPUTextureHandle before = context.GetOutputTexture();

		context.SetOutputSize({ 640.9f, 480.2f });
		CDL_EXPECT(SameHandle(context.GetOutputTexture(), before));
		CDL_EXPECT(test.Pool().IsAlive(before));
	}
	ExpectNoVulkanDebugMessages(logs);
}

CDL_TEST_CASE(RenderContext, ReleasesTheOutputTargetWhenDestroyed, Gpu)
{
	SkipUnlessRendererShadersCompiled();

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestOutputManager test;
		GPUTextureHandle target;
		{
			const RenderContext context(&test.Manager, false, kInitialSize);
			target = context.GetOutputTexture();
			CDL_CHECK(test.Pool().IsAlive(target));
		}
		CDL_EXPECT_FALSE(test.Pool().IsAlive(target));
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The scene goes straight to the swapchain, so there is nothing for a window to show, before or after a resize.
CDL_TEST_CASE(RenderContext, RenderingToTheSwapChainMakesNoOutputTarget, Gpu)
{
	SkipUnlessRendererShadersCompiled();

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestOutputManager test;
		RenderContext context(&test.Manager, true, kInitialSize);
		CDL_EXPECT(context.IsRenderingToSwapChain());
		CDL_EXPECT_FALSE(context.GetOutputTexture().IsValid());

		context.SetOutputSize({ 300.0f, 200.0f });
		CDL_EXPECT_FALSE(context.GetOutputTexture().IsValid());
	}
	ExpectNoVulkanDebugMessages(logs);
}

//////////////////////////////////////////////////////////////////////////
// End to end, in a windowed Application
//////////////////////////////////////////////////////////////////////////

// As the Sandbox shows its scene in a window: the application's RenderContext replaces the output target as the window
// resizes, the render thread matches depth to it, and the GUI samples each target in the frame the scene drew it. Fails on
// any validation message the app produces, its teardown's included.
CDL_TEST_CASE(RenderContext, AppShowsTheSceneInAGuiWindow, Display)
{
	SkipUnlessRendererShadersCompiled();

	std::vector<LogRecord> records;
	std::vector<SceneWindowApplication::Frame> frames;
	{
		// The app owns the logger, so the sink goes in before the app starts it. Windowing is probed first, so a skip can't
		// leave the sink behind for the next test's logger.
		{ WindowingBackend probe; }
		Logger::AddSink(ScopedPtr<CaptureSink>::Create(records));

		ApplicationSpecification spec = WindowedSpec();
		spec.RenderSpec = OutputTargetRenderSpec();

		std::optional<SceneWindowApplication> app;
		EmplaceWindowedApp(app, spec);
		app->Run();
		frames = app->Frames;
	}
	RestoreLoggerDefaults();

	const std::array<glm::uvec2, 6> expectedSizes = { { { 320, 240 }, { 320, 240 }, { 200, 300 }, { 1, 1 }, { 64, 32 }, { 400, 120 } } };
	CDL_CHECK_EQ(frames.size(), expectedSizes.size());
	for (size_t i = 0; i < frames.size(); ++i)
	{
		CDL_EXPECT_MSG(frames[i].Size == expectedSizes[i], std::format("frame {} drew at {}x{}", i, frames[i].Size.x, frames[i].Size.y));
		CDL_EXPECT_MSG(frames[i].Target.IsValid(), std::format("frame {} had no output target", i));
	}
	CDL_EXPECT(SameHandle(frames[1].Target, frames[0].Target));
	CDL_EXPECT_FALSE(SameHandle(frames[2].Target, frames[1].Target));

	ExpectNoVulkanDebugMessages(records);
}
