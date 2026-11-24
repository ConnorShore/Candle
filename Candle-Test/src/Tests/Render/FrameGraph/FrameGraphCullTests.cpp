// Culling drops every pass whose writes can't reach anything that outlives the frame: an import, or a pass marked
// NeverCull. Each pass records its name as it executes, so a test checks what really ran rather than how the graph
// stores it, and every frame is submitted, so a culled pass that still records anything, or a barrier on an image it
// was never given, trips validation. The graph needs a real TexturePool, so these are Gpu tests.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "../RenderTestHelpers.h"

#include <Candle/Render/FrameGraph/FrameGraph.h>
#include <Candle/Render/Resources/TexturePool.h>

#include <algorithm>
#include <format>
#include <initializer_list>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Gpu;

namespace {

	const glm::uvec2 kOutputExtent{ 8, 4 };
	constexpr vk::Format kColorFormat = vk::Format::eR8G8B8A8Unorm;
	constexpr vk::Format kDepthFormat = vk::Format::eD16Unorm;	// The one depth format every device must support as an attachment
	const vk::ClearColorValue kClearColor(0.25f, 0.5f, 0.75f, 1.0f);

	struct TestFrameGraph : TestTextures
	{
		TestPipelineStore Pipelines{ Device, Bindless };
		FrameGraph Graph{ { .Allocator = Allocator, .BindlessTable = Bindless, .TexturePool = Pool, .PipelineStore = Pipelines } };
		TestCommands Commands{ Device };
		GPUTextureHandle OutputTarget = Pool.CreateRenderTarget({ .Extent = kOutputExtent, .Format = kColorFormat, .DebugName = "Output" });
		std::vector<std::string> Ran;	// The last frame's passes, in the order they executed

		FGTextureHandle ImportOutput() { return Graph.ImportTexture("Output", OutputTarget); }

		// For a frame with more than one import. name must outlive the frame, e.g. a string literal.
		FGTextureHandle ImportNewTarget(std::string_view name)
		{
			return Graph.ImportTexture(name, Pool.CreateRenderTarget({ .Extent = kOutputExtent, .Format = kColorFormat, .DebugName = std::string(name) }));
		}

		void RunFrame(uint64_t frame)
		{
			Graph.Compile(frame);
			ExecuteFrame();
		}

		// Waits for the GPU, so the next frame can reset the pool and the test can read what this one left
		void ExecuteFrame()
		{
			Ran.clear();
			Commands.Pool.reset();
			Commands.Buffer.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
			Graph.Execute(Device, Commands.Buffer);
			Commands.Buffer.end();
			SubmitAndWait(Device, Commands.Buffer);
		}
	};

	FGTextureSpec OutputSized(vk::Format format = kColorFormat)
	{
		return { .Size = { .Mode = FGSizeMode::RelativeToOutput }, .Format = format };
	}

	auto RecordRun(std::vector<std::string>& ran)
	{
		return [ran = &ran](const auto&, FrameGraphContext& context) { ran->emplace_back(context.GetPassName()); };
	}

	template<typename SetupFunc>
	void DeclareRasterPass(TestFrameGraph& test, std::string_view name, SetupFunc setup, bool neverCull = false)
	{
		test.Graph.AddRasterPass(name, [&setup](FrameGraphBuilder& builder, FGNoData&) { setup(builder); }, RecordRun(test.Ran), neverCull);
	}

	// Compute, so a pass that only reads has no attachments to begin rendering with
	template<typename SetupFunc>
	void DeclareComputePass(TestFrameGraph& test, std::string_view name, SetupFunc setup, bool neverCull = false)
	{
		test.Graph.AddComputePass(name, [&setup](FrameGraphBuilder& builder, FGNoData&) { setup(builder); }, RecordRun(test.Ran), neverCull);
	}

	// Samples each of reads, then clears a new transient named after the pass. name must outlive the frame, e.g. a string literal.
	FGTextureHandle DrawNewTexture(TestFrameGraph& test, std::string_view name, std::initializer_list<FGTextureHandle> reads = {}, bool neverCull = false)
	{
		FGTextureHandle output;
		DeclareRasterPass(test, name, [&](FrameGraphBuilder& builder)
			{
				for (const FGTextureHandle read : reads)
					builder.ReadTexture(read, FGUsage::SampledFragment);
				output = builder.ColorAttachment(builder.CreateTexture(name, OutputSized()), FGLoad::Clear, kClearColor);
			}, neverCull);
		return output;
	}

	// Reads each of reads, then writes a new storage transient named after the pass. name must outlive the frame.
	FGTextureHandle ComputeNewTexture(TestFrameGraph& test, std::string_view name, std::initializer_list<FGTextureHandle> reads = {},
		FGUsage readUsage = FGUsage::SampledCompute)
	{
		FGTextureHandle output;
		DeclareComputePass(test, name, [&](FrameGraphBuilder& builder)
			{
				for (const FGTextureHandle read : reads)
					builder.ReadTexture(read, readUsage);
				output = builder.WriteTexture(builder.CreateTexture(name, OutputSized()), FGUsage::StorageWriteCompute);
			});
		return output;
	}

	// Samples each of reads into the imported output
	void DrawToOutput(TestFrameGraph& test, std::string_view name, std::initializer_list<FGTextureHandle> reads = {})
	{
		const FGTextureHandle output = test.ImportOutput();
		DeclareRasterPass(test, name, [&](FrameGraphBuilder& builder)
			{
				for (const FGTextureHandle read : reads)
					builder.ReadTexture(read, FGUsage::SampledFragment);
				(void)builder.ColorAttachment(output, FGLoad::Clear, kClearColor);
			});
	}

	// Stands in for a copy out to the CPU, which the graph can't see the reader of
	void DeclareReadback(TestFrameGraph& test, std::string_view name, FGTextureHandle texture)
	{
		DeclareComputePass(test, name, [&](FrameGraphBuilder& builder) { builder.ReadTexture(texture, FGUsage::SampledCompute); }, true);
	}

	template<typename Range>
	std::string JoinNames(const Range& names)
	{
		std::string joined;
		for (const auto& name : names)
			joined += joined.empty() ? std::string(name) : std::format(", {}", name);
		return joined;
	}

	// Exactly these passes, in this order
	void ExpectRan(const std::vector<std::string>& ran, std::initializer_list<std::string_view> expected)
	{
		CDL_EXPECT_MSG(std::ranges::equal(ran, expected), std::format("ran [{}], expected [{}]", JoinNames(ran), JoinNames(expected)));
	}

	// The same passes every frame, with only the composite's read changing, as when a debug view is toggled at runtime
	void RunDebugViewFrame(TestFrameGraph& test, uint64_t frame, bool showDebugView)
	{
		test.Graph.Reset(kOutputExtent);
		const FGTextureHandle debugView = DrawNewTexture(test, "DebugView");
		const FGTextureHandle output = test.ImportOutput();
		DeclareRasterPass(test, "Composite", [&](FrameGraphBuilder& builder)
			{
				if (showDebugView)
					builder.ReadTexture(debugView, FGUsage::SampledFragment);
				(void)builder.ColorAttachment(output, FGLoad::Clear, kClearColor);
			});
		test.RunFrame(frame);
	}

	// Null when the named pass has no barrier for any version of texture
	const FGBarrier* FindBarrier(const FGCompiledGraph& plan, std::string_view pass, FGTextureHandle texture)
	{
		const auto compiled = std::ranges::find(plan.Passes, pass, &FGCompiledPass::Name);
		CDL_CHECK_MSG(compiled != plan.Passes.end(), std::format("{} wasn't compiled", pass));

		const std::span<const FGBarrier> barriers = std::span(plan.Barriers).subspan(compiled->BarrierStartIndex, compiled->BarrierCount);
		const auto found = std::ranges::find_if(barriers, [texture](const FGBarrier& barrier) { return barrier.Texture.IsSameResource(texture); });
		return found == barriers.end() ? nullptr : &*found;
	}

}

// Without this, every other test here passes against a graph that culls everything
CDL_TEST_CASE(FrameGraphCull, PassWritingAnImportIsKept, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		test.Graph.Reset(kOutputExtent);

		DrawToOutput(test, "Scene");
		test.RunFrame(1);

		ExpectRan(test.Ran, { "Scene" });
	}
	ExpectNoVulkanDebugMessages(logs);
}

// A debug view nobody turned on: its pass is still added every frame, and culling is what keeps it off the GPU
CDL_TEST_CASE(FrameGraphCull, PassNothingReadsIsCulled, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		test.Graph.Reset(kOutputExtent);

		DrawNewTexture(test, "DebugView");
		DrawToOutput(test, "Scene");
		test.RunFrame(1);

		ExpectRan(test.Ran, { "Scene" });
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Bloom switched off: the tonemap stops reading the chain's end, and culling has to follow that back through every
// pass, not just drop the last one
CDL_TEST_CASE(FrameGraphCull, ChainNothingReadsIsCulledWhole, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		test.Graph.Reset(kOutputExtent);

		const FGTextureHandle downsample = DrawNewTexture(test, "BloomDownsample");
		const FGTextureHandle blur = DrawNewTexture(test, "BloomBlur", { downsample });
		DrawNewTexture(test, "BloomUpsample", { blur });
		DrawToOutput(test, "Tonemap");
		test.RunFrame(1);

		ExpectRan(test.Ran, { "Tonemap" });
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Lighting samples the shadow map, so the shadow pass is kept through it, and still runs first
CDL_TEST_CASE(FrameGraphCull, PassAKeptPassReadsIsKept, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		test.Graph.Reset(kOutputExtent);

		const FGTextureHandle shadows = DrawNewTexture(test, "Shadows");
		DrawToOutput(test, "Lighting", { shadows });
		test.RunFrame(1);

		ExpectRan(test.Ran, { "Shadows", "Lighting" });
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The normals view reads the GBuffer too, but only lighting reaches the output. Its read mustn't keep it alive, and
// dropping it mustn't take the GBuffer with it.
CDL_TEST_CASE(FrameGraphCull, SharedProducerKeepsOnlyTheReadersThatAreUsed, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		test.Graph.Reset(kOutputExtent);

		const FGTextureHandle gbuffer = DrawNewTexture(test, "GBuffer");
		DrawToOutput(test, "Lighting", { gbuffer });
		DrawNewTexture(test, "NormalsDebugView", { gbuffer });
		test.RunFrame(1);

		ExpectRan(test.Ran, { "GBuffer", "Lighting" });
	}
	ExpectNoVulkanDebugMessages(logs);
}

// For work the graph can't see the result of, like a pass writing through a descriptor it never declared
CDL_TEST_CASE(FrameGraphCull, NeverCullPassIsKeptWithNoReaders, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		test.Graph.Reset(kOutputExtent);

		DrawNewTexture(test, "Untracked", {}, true);
		test.RunFrame(1);

		ExpectRan(test.Ran, { "Untracked" });
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Object picking on a click: nothing in the graph reads the ID buffer but the readback copying it out to the CPU
CDL_TEST_CASE(FrameGraphCull, NeverCullPassKeepsWhatItReads, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		test.Graph.Reset(kOutputExtent);

		const FGTextureHandle ids = DrawNewTexture(test, "PickingIds");
		DeclareReadback(test, "PickingReadback", ids);
		test.RunFrame(1);

		ExpectRan(test.Ran, { "PickingIds", "PickingReadback" });
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Debug draw loads the scene and draws over it, as Renderer's does, so the tonemap reads what both of them wrote
CDL_TEST_CASE(FrameGraphCull, LoadingOverwriteKeepsThePassBefore, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		test.Graph.Reset(kOutputExtent);

		FGTextureHandle color = DrawNewTexture(test, "Scene");
		DeclareRasterPass(test, "DebugDraw", [&](FrameGraphBuilder& builder) 
			{ 
				color = builder.ColorAttachment(color, FGLoad::Load, kClearColor); 
			});
		DrawToOutput(test, "Tonemap", { color });
		test.RunFrame(1);

		ExpectRan(test.Ran, { "Scene", "DebugDraw", "Tonemap" });
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The scene clears over the background before anything reads it, so nothing the background drew reaches the output.
// This pins a graph that reads the load op; one treating every write to an existing texture as read-modify-write keeps
// the background, which is also a defensible choice.
CDL_TEST_CASE(FrameGraphCull, ClearingOverwriteCullsThePassBefore, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		test.Graph.Reset(kOutputExtent);

		FGTextureHandle color = DrawNewTexture(test, "Background");
		DeclareRasterPass(test, "Scene", [&](FrameGraphBuilder& builder) { color = builder.ColorAttachment(color, FGLoad::Clear, kClearColor); });
		DrawToOutput(test, "Tonemap", { color });
		test.RunFrame(1);

		ExpectRan(test.Ran, { "Scene", "Tonemap" });
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Nothing samples the depth buffer, but the scene still renders into it, so a kept pass's unread outputs need images
CDL_TEST_CASE(FrameGraphCull, KeptPassStillGetsItsUnreadOutputs, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		test.Graph.Reset(kOutputExtent);

		const FGTextureHandle output = test.ImportOutput();
		FGTextureHandle depth;
		DeclareRasterPass(test, "Scene", [&](FrameGraphBuilder& builder)
			{
				(void)builder.ColorAttachment(output, FGLoad::Clear, kClearColor);
				depth = builder.DepthAttachment(builder.CreateTexture("Depth", OutputSized(kDepthFormat)), FGLoad::Clear);
			});
		test.RunFrame(1);

		ExpectRan(test.Ran, { "Scene" });
		CDL_EXPECT(test.Graph.GetTexture(depth).Image);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The point of culling the pass is that its texture costs nothing either
CDL_TEST_CASE(FrameGraphCull, CulledPassesTransientGetsNoImage, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		test.Graph.Reset(kOutputExtent);

		const FGTextureHandle debugView = DrawNewTexture(test, "DebugView");
		DrawToOutput(test, "Scene");
		test.Graph.Compile(1);

		const FGTextureEntry& entry = test.Graph.GetTexture(debugView);
		CDL_EXPECT_MSG(!entry.Image, "the culled pass's texture was still given an image");
		CDL_EXPECT_EQ(entry.CacheIndex, std::numeric_limits<uint32_t>::max());
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Catches cull state left over from the frame before
CDL_TEST_CASE(FrameGraphCull, CullingIsDecidedAgainEachFrame, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;

		RunDebugViewFrame(test, 1, true);
		ExpectRan(test.Ran, { "DebugView", "Composite" });
		RunDebugViewFrame(test, 2, false);
		ExpectRan(test.Ran, { "Composite" });
		RunDebugViewFrame(test, 3, true);
		ExpectRan(test.Ran, { "DebugView", "Composite" });
	}
	ExpectNoVulkanDebugMessages(logs);
}

// A debug view switched off stops costing memory once the cache gives up on its image, so culling a pass has to stop
// it asking for one rather than just skip it at execute
CDL_TEST_CASE(FrameGraphCull, CulledPassesTransientIsFreedOnceTheCacheGivesUpOnIt, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		const auto runFrame = [&test](uint64_t frame, bool showDebugView)
			{
				RunDebugViewFrame(test, frame, showDebugView);
				test.Graph.EvictUnusedResources(frame, test.Device.GetLastSubmittedTimelineValue(QueueType::Graphics),
					test.Device.GetCompletedTimelineValue(QueueType::Graphics));
			};

		runFrame(1, true);
		const uint32_t withDebugView = LiveAllocations(test.Allocator);
		for (uint64_t frame = 2; frame <= 2 + ResourceCache::kMaxUnusedFramesBeforeEviction; ++frame)
			runFrame(frame, false);

		ExpectRan(test.Ran, { "Composite" });
		CDL_EXPECT_EQ(LiveAllocations(test.Allocator), withDebugView - 1);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Nothing reaches an import, so every pass goes, and the frame still has to record and submit cleanly
CDL_TEST_CASE(FrameGraphCull, FrameWithNothingKeptRunsNoPasses, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		test.Graph.Reset(kOutputExtent);

		const FGTextureHandle debugView = DrawNewTexture(test, "DebugView");
		DrawNewTexture(test, "DebugViewBlur", { debugView });
		test.RunFrame(1);

		ExpectRan(test.Ran, {});
	}
	ExpectNoVulkanDebugMessages(logs);
}

// SSAO and lighting both read the GBuffer, and lighting reads SSAO too, so the GBuffer is reached twice. It still runs
// once, ahead of both.
CDL_TEST_CASE(FrameGraphCull, ProducerTwoKeptPassesReadRunsOnce, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		test.Graph.Reset(kOutputExtent);

		const FGTextureHandle gbuffer = DrawNewTexture(test, "GBuffer");
		const FGTextureHandle ssao = DrawNewTexture(test, "SSAO", { gbuffer });
		DrawToOutput(test, "Lighting", { gbuffer, ssao });
		test.RunFrame(1);

		ExpectRan(test.Ran, { "GBuffer", "SSAO", "Lighting" });
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The editor's scene window: the scene draws into the viewport target and the editor samples it into the window. The
// scene is kept for writing an import and again for the editor's read, and still runs once.
CDL_TEST_CASE(FrameGraphCull, ImportWriterAnotherKeptPassReadsRunsOnce, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		test.Graph.Reset(kOutputExtent);

		FGTextureHandle viewport = test.ImportNewTarget("Viewport");
		DeclareRasterPass(test, "Scene", [&](FrameGraphBuilder& builder) { viewport = builder.ColorAttachment(viewport, FGLoad::Clear, kClearColor); });
		DrawToOutput(test, "Editor", { viewport });
		test.RunFrame(1);

		ExpectRan(test.Ran, { "Scene", "Editor" });
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The swapchain frame Renderer draws: the scene clears the back buffer and the GUI loads it to draw on top. Only the
// GUI's write is the import's last, so the scene is kept through the load.
CDL_TEST_CASE(FrameGraphCull, ImportWriteALaterPassLoadsIsKept, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		test.Graph.Reset(kOutputExtent);

		FGTextureHandle output = test.ImportOutput();
		DeclareRasterPass(test, "Scene", [&](FrameGraphBuilder& builder) { output = builder.ColorAttachment(output, FGLoad::Clear, kClearColor); });
		DeclareRasterPass(test, "Gui", [&](FrameGraphBuilder& builder) { output = builder.ColorAttachment(output, FGLoad::Load, kClearColor); });
		test.RunFrame(1);

		ExpectRan(test.Ran, { "Scene", "Gui" });
	}
	ExpectNoVulkanDebugMessages(logs);
}

// A splash screen left in after the scene took over the window: the scene clears the output again, so nothing the
// splash drew survives the frame. Writing an import only keeps the import's last write.
CDL_TEST_CASE(FrameGraphCull, ImportWriteALaterClearReplacesIsCulled, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		test.Graph.Reset(kOutputExtent);

		FGTextureHandle output = test.ImportOutput();
		DeclareRasterPass(test, "Splash", [&](FrameGraphBuilder& builder) { output = builder.ColorAttachment(output, FGLoad::Clear, kClearColor); });
		DeclareRasterPass(test, "Scene", [&](FrameGraphBuilder& builder) { output = builder.ColorAttachment(output, FGLoad::Clear, kClearColor); });
		test.RunFrame(1);

		ExpectRan(test.Ran, { "Scene" });
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The composite writes every pixel, so it skips the clear. DontCare throws away what was there as surely as a clear
// does, so the pass that drew it is dead.
CDL_TEST_CASE(FrameGraphCull, DontCareOverwriteCullsThePassBefore, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		test.Graph.Reset(kOutputExtent);

		FGTextureHandle color = DrawNewTexture(test, "Background");
		DeclareRasterPass(test, "Composite", [&](FrameGraphBuilder& builder) { color = builder.ColorAttachment(color, FGLoad::DontCare, kClearColor); });
		DrawToOutput(test, "Tonemap", { color });
		test.RunFrame(1);

		ExpectRan(test.Ran, { "Composite", "Tonemap" });
	}
	ExpectNoVulkanDebugMessages(logs);
}

// A depth prepass: the scene loads the depth it laid down and tests against it
CDL_TEST_CASE(FrameGraphCull, DepthPrepassIsKeptByTheSceneLoadingItsDepth, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		test.Graph.Reset(kOutputExtent);

		FGTextureHandle depth;
		DeclareRasterPass(test, "DepthPrepass", [&](FrameGraphBuilder& builder)
			{
				depth = builder.DepthAttachment(builder.CreateTexture("Depth", OutputSized(kDepthFormat)), FGLoad::Clear);
			});
		const FGTextureHandle output = test.ImportOutput();
		DeclareRasterPass(test, "Scene", [&](FrameGraphBuilder& builder)
			{
				(void)builder.ColorAttachment(output, FGLoad::Clear, kClearColor);
				(void)builder.DepthAttachment(depth, FGLoad::Load);
			});
		test.RunFrame(1);

		ExpectRan(test.Ran, { "DepthPrepass", "Scene" });
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Renderer's compute pass: it writes a storage texture the scene samples, so a WriteTexture has to count as producing
// what a later pass reads, not just an attachment
CDL_TEST_CASE(FrameGraphCull, ComputePassAKeptPassSamplesIsKept, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		test.Graph.Reset(kOutputExtent);

		const FGTextureHandle particles = ComputeNewTexture(test, "Particles");
		DrawToOutput(test, "Scene", { particles });
		test.RunFrame(1);

		ExpectRan(test.Ran, { "Particles", "Scene" });
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Fog composited into the scene color in place, through a storage image the shader reads and writes. The graph can't see
// whether a storage write reads first, so the scene that drew what the fog reads has to stay.
CDL_TEST_CASE(FrameGraphCull, InPlaceStorageWriteKeepsThePassBefore, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		test.Graph.Reset(kOutputExtent);

		FGTextureHandle color = DrawNewTexture(test, "Scene");
		DeclareComputePass(test, "VolumetricFog", [&](FrameGraphBuilder& builder) { color = builder.WriteTexture(color, FGUsage::StorageWriteCompute); });
		DrawToOutput(test, "Tonemap", { color });
		test.RunFrame(1);

		ExpectRan(test.Ran, { "Scene", "VolumetricFog", "Tonemap" });
	}
	ExpectNoVulkanDebugMessages(logs);
}

// A debug overlay drawn into the scene color after the tonemap has read it. Nothing reads what it drew, so it goes, but
// the scene and tonemap still use the texture, so it still needs an image.
CDL_TEST_CASE(FrameGraphCull, TransientKeepsItsImageWhenOnlyItsLastWriterIsCulled, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		test.Graph.Reset(kOutputExtent);

		const FGTextureHandle color = DrawNewTexture(test, "Scene");
		DrawToOutput(test, "Tonemap", { color });
		DeclareRasterPass(test, "DebugOverlay", [&](FrameGraphBuilder& builder) { (void)builder.ColorAttachment(color, FGLoad::Load, kClearColor); });
		test.Graph.Compile(1);

		// Before executing, since recording the kept passes against a missing image crashes rather than fails
		CDL_CHECK_MSG(test.Graph.GetTexture(color).Image, "the kept passes' texture has no image");
		test.ExecuteFrame();

		ExpectRan(test.Ran, { "Scene", "Tonemap" });
	}
	ExpectNoVulkanDebugMessages(logs);
}

// A histogram debug view reads the GBuffer as a storage image, but it's culled. Its usage mustn't reach the image: it
// changes the cache key, and plenty of formats, the sRGB ones among them, can't be storage images at all.
CDL_TEST_CASE(FrameGraphCull, CulledPassesUsageIsLeftOffTheTexture, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		test.Graph.Reset(kOutputExtent);

		const FGTextureHandle gbuffer = DrawNewTexture(test, "GBuffer");
		DrawToOutput(test, "Lighting", { gbuffer });
		ComputeNewTexture(test, "HistogramDebugView", { gbuffer }, FGUsage::StorageReadCompute);
		test.RunFrame(1);

		ExpectRan(test.Ran, { "GBuffer", "Lighting" });
		const vk::ImageUsageFlags usage = test.Graph.GetTexture(gbuffer).Desc.Usage;
		const vk::ImageUsageFlags expected = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
		CDL_EXPECT_MSG(usage == expected, std::format("usage is {}, expected {}", vk::to_string(usage), vk::to_string(expected)));
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The normals view samples the GBuffer between its write and lighting's read, but it's culled. Had its read counted,
// lighting's read would look like a second read in the same layout and get no barrier, and could sample before the
// GBuffer's writes land.
CDL_TEST_CASE(FrameGraphCull, CulledReaderLeavesNoTraceInTheBarriers, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		test.Graph.Reset(kOutputExtent);

		const FGTextureHandle gbuffer = DrawNewTexture(test, "GBuffer");
		ComputeNewTexture(test, "NormalsDebugView", { gbuffer });
		DrawToOutput(test, "Lighting", { gbuffer });
		test.RunFrame(1);

		ExpectRan(test.Ran, { "GBuffer", "Lighting" });
		const FGBarrier* barrier = FindBarrier(test.Graph.GetCompiledGraph(), "Lighting", gbuffer);
		CDL_CHECK_MSG(barrier, "lighting's read of the GBuffer has no barrier");
		CDL_EXPECT_EQ(barrier->FromState.Layout, vk::ImageLayout::eColorAttachmentOptimal);
		CDL_EXPECT(barrier->FromState.Access & vk::AccessFlagBits2::eColorAttachmentWrite);
	}
	ExpectNoVulkanDebugMessages(logs);
}
