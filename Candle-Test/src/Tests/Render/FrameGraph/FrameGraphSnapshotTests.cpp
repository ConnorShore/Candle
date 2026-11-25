// FrameGraph::CaptureSnapshot, the copy of a compiled graph the render graph viewer draws from. It has to match what the graph
// compiled, and own every name in it, since it's read on the main thread frames after the graph has reset. Compiling needs a
// real TexturePool and ResourceCache, so these are Gpu tests, but nothing is executed.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "../RenderTestHelpers.h"

#include <Candle/Render/FrameGraph/FrameGraph.h>
#include <Candle/Render/Resources/TexturePool.h>

#include <algorithm>
#include <format>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Gpu;

namespace {

	const glm::uvec2 kOutputExtent{ 8, 4 };
	constexpr vk::Format kColorFormat = vk::Format::eR8G8B8A8Unorm;
	const vk::ClearColorValue kClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	constexpr auto kNoExecute = [](const FGNoData&, FrameGraphContext&) {};

	struct TestFrameGraph : TestTextures
	{
		TestPipelineStore Pipelines{ Device, Bindless };
		FrameGraph Graph{ { .Allocator = Allocator, .BindlessTable = Bindless, .TexturePool = Pool, .PipelineStore = Pipelines } };
		GPUTextureHandle OutputTarget = Pool.CreateRenderTarget({ .Extent = kOutputExtent, .Format = kColorFormat, .DebugName = "Output" });
	};

	FGTextureSpec OutputSized()
	{
		return { .Size = { .Mode = FGSizeMode::RelativeToOutput }, .Format = kColorFormat };
	}

	// Samples each of reads, then clears a new transient named texture. texture must outlive the frame, e.g. a string literal.
	FGTextureHandle DrawNewTexture(TestFrameGraph& test, std::string_view pass, std::string_view texture, std::initializer_list<FGTextureHandle> reads = {})
	{
		FGTextureHandle output;
		test.Graph.AddRasterPass(pass, [&](FrameGraphBuilder& builder, FGNoData&)
			{
				for (const FGTextureHandle read : reads)
					builder.ReadTexture(read, FGUsage::SampledFragment);
				output = builder.ColorAttachment(builder.CreateTexture(texture, OutputSized()), FGLoad::Clear, kClearColor);
			}, kNoExecute);
		return output;
	}

	// Samples each of reads into the imported output
	void DrawToOutput(TestFrameGraph& test, std::string_view pass, std::initializer_list<FGTextureHandle> reads = {})
	{
		const FGTextureHandle output = test.Graph.ImportTexture("Output", test.OutputTarget);
		test.Graph.AddRasterPass(pass, [&](FrameGraphBuilder& builder, FGNoData&)
			{
				for (const FGTextureHandle read : reads)
					builder.ReadTexture(read, FGUsage::SampledFragment);
				(void)builder.ColorAttachment(output, FGLoad::Clear, kClearColor);
			}, kNoExecute);
	}

	// A deferred frame with a debug view nobody turned on: GBuffer feeds Lighting, which writes the output, and DebugView is culled
	void BuildDeferredFrame(TestFrameGraph& test)
	{
		test.Graph.Reset(kOutputExtent);
		const FGTextureHandle albedo = DrawNewTexture(test, "GBuffer", "Albedo");
		DrawNewTexture(test, "DebugView", "DebugOverlay");
		DrawToOutput(test, "Lighting", { albedo });
		test.Graph.Compile(1);
	}

	const FrameGraphSnapshot::Texture& TextureNamed(const FrameGraphSnapshot& snapshot, std::string_view name)
	{
		const auto it = std::ranges::find(snapshot.Textures, name, &FrameGraphSnapshot::Texture::Name);
		CDL_CHECK_MSG(it != snapshot.Textures.end(), std::format("the snapshot has no texture named {}", name));
		return *it;
	}

	std::span<const FGAccessEntry> AccessesOf(const FrameGraphSnapshot& snapshot, uint32_t pass)
	{
		return std::span(snapshot.Accesses).subspan(snapshot.Passes[pass].AccessStart, snapshot.Passes[pass].AccessCount);
	}

}

CDL_TEST_CASE(FrameGraphSnapshot, ListsEveryPassInDeclarationOrderWithCulledOnesMarked, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		BuildDeferredFrame(test);

		FrameGraphSnapshot snapshot;
		test.Graph.CaptureSnapshot(snapshot);

		CDL_CHECK_EQ(snapshot.Passes.size(), size_t{ 3 });
		CDL_EXPECT_EQ(snapshot.Passes[0].Name, std::string("GBuffer"));
		CDL_EXPECT_EQ(snapshot.Passes[1].Name, std::string("DebugView"));
		CDL_EXPECT_EQ(snapshot.Passes[2].Name, std::string("Lighting"));
		CDL_EXPECT(snapshot.Passes[0].Live);
		CDL_EXPECT_FALSE(snapshot.Passes[1].Live);
		CDL_EXPECT(snapshot.Passes[2].Live);
		CDL_EXPECT(snapshot.OutputExtent == kOutputExtent);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Lighting's read of the G-buffer is what kept GBuffer alive, so the viewer needs which pass wrote it as well as the version
CDL_TEST_CASE(FrameGraphSnapshot, AccessesKeepTheirTextureVersionLoadAndWriter, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		BuildDeferredFrame(test);

		FrameGraphSnapshot snapshot;
		test.Graph.CaptureSnapshot(snapshot);

		const std::span<const FGAccessEntry> gbuffer = AccessesOf(snapshot, 0);
		CDL_CHECK_EQ(gbuffer.size(), size_t{ 1 });
		CDL_EXPECT_EQ(snapshot.Textures[gbuffer[0].ResourceIndex].Name, std::string("Albedo"));
		CDL_EXPECT(gbuffer[0].Write);
		CDL_EXPECT_EQ(gbuffer[0].LoadOp, FGLoad::Clear);
		CDL_EXPECT_EQ(gbuffer[0].Version, 0u);

		const std::span<const FGAccessEntry> lighting = AccessesOf(snapshot, 2);
		CDL_CHECK_EQ(lighting.size(), size_t{ 2 });
		CDL_EXPECT_EQ(snapshot.Textures[lighting[0].ResourceIndex].Name, std::string("Albedo"));
		CDL_EXPECT_FALSE(lighting[0].Write);
		CDL_EXPECT_EQ(lighting[0].Usage, FGUsage::SampledFragment);
		CDL_EXPECT_EQ(lighting[0].Version, 1u);
		CDL_EXPECT_EQ(lighting[0].SrcPassIndex, 0u);
		CDL_EXPECT_EQ(snapshot.Textures[lighting[1].ResourceIndex].Name, std::string("Output"));
		CDL_EXPECT(lighting[1].Write);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// A transient's usage only comes from live passes, so one that only a culled pass touched is never allocated
CDL_TEST_CASE(FrameGraphSnapshot, TexturesCarryWhatCompileResolved, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		BuildDeferredFrame(test);

		FrameGraphSnapshot snapshot;
		test.Graph.CaptureSnapshot(snapshot);

		const FrameGraphSnapshot::Texture& albedo = TextureNamed(snapshot, "Albedo");
		CDL_EXPECT_FALSE(albedo.Imported);
		CDL_EXPECT(albedo.Extent == kOutputExtent);
		CDL_EXPECT_EQ(albedo.Format, kColorFormat);
		CDL_EXPECT(albedo.Usage == (vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled));
		CDL_EXPECT_EQ(albedo.LatestVersion, 1u);
		CDL_EXPECT_NE(albedo.CacheIndex, FrameGraphSnapshot::kNone);
		CDL_EXPECT_NE(albedo.BindlessIndex, FrameGraphSnapshot::kNone);

		const FrameGraphSnapshot::Texture& overlay = TextureNamed(snapshot, "DebugOverlay");
		CDL_EXPECT(overlay.Usage == vk::ImageUsageFlags{});
		CDL_EXPECT_EQ(overlay.CacheIndex, FrameGraphSnapshot::kNone);

		CDL_EXPECT(TextureNamed(snapshot, "Output").Imported);
	}
	ExpectNoVulkanDebugMessages(logs);
}

CDL_TEST_CASE(FrameGraphSnapshot, BarriersMatchTheCompiledGraph, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		BuildDeferredFrame(test);

		FrameGraphSnapshot snapshot;
		test.Graph.CaptureSnapshot(snapshot);

		const FGCompiledGraph& compiled = test.Graph.GetCompiledGraph();
		CDL_CHECK_EQ(snapshot.Barriers.size(), compiled.Barriers.size());
		CDL_EXPECT_EQ(snapshot.FinalBarriers.size(), compiled.FinalBarriers.size());
		for (const FGCompiledPass& pass : compiled.Passes)
		{
			CDL_NOTE(std::string(pass.Name));
			CDL_EXPECT_EQ(snapshot.Passes[pass.PassIndex].BarrierStart, pass.BarrierStartIndex);
			CDL_EXPECT_EQ(snapshot.Passes[pass.PassIndex].BarrierCount, pass.BarrierCount);
		}
		for (size_t i = 0; i < compiled.Barriers.size(); ++i)
		{
			CDL_EXPECT_EQ(snapshot.Barriers[i].Texture.Index, compiled.Barriers[i].Texture.Index);
			CDL_EXPECT_EQ(snapshot.Barriers[i].FromState.Layout, compiled.Barriers[i].FromState.Layout);
			CDL_EXPECT_EQ(snapshot.Barriers[i].ToState.Layout, compiled.Barriers[i].ToState.Layout);
		}

		CDL_EXPECT_EQ(snapshot.Passes[1].BarrierCount, 0u);	// Culled
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The graph only promises texture names live for the frame, but the snapshot is read frames later
CDL_TEST_CASE(FrameGraphSnapshot, OwnsItsNames, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		std::string textureName = "Bloom";

		test.Graph.Reset(kOutputExtent);
		const FGTextureHandle bloom = DrawNewTexture(test, "BloomDownsample", textureName);
		DrawToOutput(test, "Tonemap", { bloom });
		test.Graph.Compile(1);

		FrameGraphSnapshot snapshot;
		test.Graph.CaptureSnapshot(snapshot);
		textureName = "Overwritten";
		test.Graph.Reset(kOutputExtent);

		CDL_EXPECT_EQ(snapshot.Passes[0].Name, std::string("BloomDownsample"));
		CDL_EXPECT_EQ(snapshot.Textures[0].Name, std::string("Bloom"));
	}
	ExpectNoVulkanDebugMessages(logs);
}

// One snapshot is reused every frame, so a frame with fewer passes mustn't leave the last frame's behind
CDL_TEST_CASE(FrameGraphSnapshot, RecapturingASmallerGraphDropsWhatItNoLongerHas, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		BuildDeferredFrame(test);

		FrameGraphSnapshot snapshot;
		test.Graph.CaptureSnapshot(snapshot);

		test.Graph.Reset(kOutputExtent);
		DrawToOutput(test, "Clear");
		test.Graph.Compile(2);
		test.Graph.CaptureSnapshot(snapshot);

		CDL_CHECK_EQ(snapshot.Passes.size(), size_t{ 1 });
		CDL_EXPECT_EQ(snapshot.Passes[0].Name, std::string("Clear"));
		CDL_EXPECT_EQ(snapshot.Textures.size(), size_t{ 1 });
		CDL_EXPECT_EQ(snapshot.Accesses.size(), size_t{ 1 });
	}
	ExpectNoVulkanDebugMessages(logs);
}

// What the preview pass looks a transient up by, so it has to be the version every write so far has produced
CDL_TEST_CASE(FrameGraphSnapshot, FindTextureReturnsTheLatestVersion, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		test.Graph.Reset(kOutputExtent);

		const FGTextureHandle albedo = DrawNewTexture(test, "GBuffer", "Albedo");
		FGTextureHandle decaled;
		test.Graph.AddRasterPass("Decals", [&](FrameGraphBuilder& builder, FGNoData&)
			{
				decaled = builder.ColorAttachment(albedo, FGLoad::Load, kClearColor);
			}, kNoExecute);

		const std::optional<FGTextureHandle> found = test.Graph.FindTexture("Albedo");
		CDL_CHECK(found.has_value());
		CDL_EXPECT(*found == decaled);
		CDL_EXPECT_EQ(found->Version, 2u);

		CDL_EXPECT_FALSE(test.Graph.FindTexture("Missing").has_value());
	}
	ExpectNoVulkanDebugMessages(logs);
}
