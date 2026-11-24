// Execute records what Compile planned: each pass's barriers, a render pass over a raster pass's attachments with the
// pass's own commands inside it, then the final barriers. These run real frames and read back what landed, so they're
// Gpu tests.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "../RenderTestHelpers.h"

#include <Candle/Render/FrameGraph/FrameGraph.h>
#include <Candle/Render/Resources/TexturePool.h>

#include <format>
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

	// Exact in UNORM, so they read back as kRed and kBlue
	const vk::ClearColorValue kClearRed(1.0f, 0.0f, 0.0f, 1.0f);
	const vk::ClearColorValue kClearBlue(0.0f, 0.0f, 1.0f, 1.0f);

	constexpr auto kNoExecute = [](const auto&, FrameGraphContext&) {};

	struct TestFrameGraph : TestTextures
	{
		TestPipelineStore Pipelines{ Device, Bindless };
		FrameGraph Graph{ { .Allocator = Allocator, .BindlessTable = Bindless, .TexturePool = Pool, .PipelineStore = Pipelines } };
		TestCommands Commands{ Device };

		// One the test can copy back out. name must outlive the frame, e.g. a string literal.
		FGTextureHandle ImportReadableTarget(std::string_view name)
		{
			const GPUTextureHandle target = Pool.CreateRenderTarget({ .Extent = kOutputExtent, .Format = kColorFormat,
				.Usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc,
				.DebugName = std::string(name) });
			return Graph.ImportTexture(name, target);
		}

		// Waits for the GPU, so the test can read back what the frame left
		void RunFrame(uint64_t frame)
		{
			Graph.Compile(frame);

			Commands.Pool.reset();
			Commands.Buffer.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
			Graph.Execute(Device, Commands.Buffer);
			Commands.Buffer.end();
			SubmitAndWait(Device, Commands.Buffer);
		}

		// From the state the final barrier left it in, so a frame that left it anywhere else fails validation
		std::vector<Texel> ReadBack(FGTextureHandle texture)
		{
			const FGTextureEntry& entry = Graph.GetTexture(texture);
			return ToTexels(ReadBackImage(*this, entry.Image, { entry.Desc.Extent.x, entry.Desc.Extent.y }, entry.FinalState));
		}
	};

	FGTextureSpec OutputSized(float scale = 1.0f)
	{
		return { .Size = { .Mode = FGSizeMode::RelativeToOutput, .Scale = scale }, .Format = kColorFormat };
	}

	void ExpectEveryTexel(std::span<const Texel> texels, const Texel& expected, std::string_view label)
	{
		for (size_t i = 0; i < texels.size(); ++i)
		{
			if (texels[i] != expected)
			{
				CDL_EXPECT_MSG(false, std::format("{}: texel {} is {}, expected {} everywhere", label, i, texels[i], expected));
				return;
			}
		}
	}

	struct OutputData
	{
		FGTextureHandle Output;
	};

}

CDL_TEST_CASE(FrameGraphExecute, ClearedAttachmentHoldsItsClearValue, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		test.Graph.Reset(kOutputExtent);

		const FGTextureHandle target = test.ImportReadableTarget("Target");
		test.Graph.AddRasterPass("Clear", [&](FrameGraphBuilder& builder, FGNoData&) { (void)builder.ColorAttachment(target, FGLoad::Clear, kClearRed); }, kNoExecute);
		test.RunFrame(1);

		ExpectEveryTexel(test.ReadBack(target), kRed, "target");
	}
	ExpectNoVulkanDebugMessages(logs);
}

// A GBuffer clears each of its targets to a value of its own, so each attachment has to carry its own, in order
CDL_TEST_CASE(FrameGraphExecute, EachColorAttachmentGetsItsOwnClearValue, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		test.Graph.Reset(kOutputExtent);

		const FGTextureHandle albedo = test.ImportReadableTarget("Albedo");
		const FGTextureHandle normals = test.ImportReadableTarget("Normals");
		test.Graph.AddRasterPass("GBuffer", [&](FrameGraphBuilder& builder, FGNoData&)
			{
				(void)builder.ColorAttachment(albedo, FGLoad::Clear, kClearRed);
				(void)builder.ColorAttachment(normals, FGLoad::Clear, kClearBlue);
			}, kNoExecute);
		test.RunFrame(1);

		ExpectEveryTexel(test.ReadBack(albedo), kRed, "albedo");
		ExpectEveryTexel(test.ReadBack(normals), kBlue, "normals");
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The overlay loads the scene's target and draws nothing, so what the scene cleared it to has to come through. The
// overlay's own clear value is blue, so an attachment that cleared instead of loading shows.
CDL_TEST_CASE(FrameGraphExecute, LoadedAttachmentKeepsWhatThePassBeforeWrote, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		test.Graph.Reset(kOutputExtent);

		FGTextureHandle target = test.ImportReadableTarget("Target");
		test.Graph.AddRasterPass("Scene", [&](FrameGraphBuilder& builder, FGNoData&) { target = builder.ColorAttachment(target, FGLoad::Clear, kClearRed); }, kNoExecute);
		test.Graph.AddRasterPass("Overlay", [&](FrameGraphBuilder& builder, FGNoData&) { target = builder.ColorAttachment(target, FGLoad::Load, kClearBlue); }, kNoExecute);
		test.RunFrame(1);

		ExpectEveryTexel(test.ReadBack(target), kRed, "target");
	}
	ExpectNoVulkanDebugMessages(logs);
}

// A half-resolution pass sets its viewport from this, so it's the size of the pass's attachments rather than the output
CDL_TEST_CASE(FrameGraphExecute, ContextGivesThePassItsAttachmentsExtent, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		test.Graph.Reset(kOutputExtent);

		vk::Extent2D seen{ 0, 0 };
		test.Graph.AddRasterPass("HalfRes",
			[](FrameGraphBuilder& builder, FGNoData&) { (void)builder.ColorAttachment(builder.CreateTexture("HalfRes", OutputSized(0.5f)), FGLoad::Clear, kClearRed); },
			[seen = &seen](const FGNoData&, FrameGraphContext& context) { *seen = context.GetRenderExtent(); }, true);
		test.RunFrame(1);

		CDL_EXPECT_EQ(seen.width, kOutputExtent.x / 2);
		CDL_EXPECT_EQ(seen.height, kOutputExtent.y / 2);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Renderer's compute pass binds its output by the bindless index it looks up at execute, through the handle setup left in
// its pass data, so by then the transient has its image and slot
CDL_TEST_CASE(FrameGraphExecute, ContextResolvesTheHandleSetupLeftInThePassData, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		test.Graph.Reset(kOutputExtent);

		FGTextureEntry seen;
		const OutputData& passData = test.Graph.AddComputePass<OutputData>("Particles",
			[](FrameGraphBuilder& builder, OutputData& data)
			{
				data.Output = builder.WriteTexture(builder.CreateTexture("Particles", OutputSized()), FGUsage::StorageWriteCompute);
			},
			[seen = &seen](const OutputData& data, FrameGraphContext& context) { *seen = context.GetTexture(data.Output); }, true);
		test.RunFrame(1);

		const FGTextureEntry& entry = test.Graph.GetTexture(passData.Output);
		CDL_EXPECT(seen.Image);
		CDL_EXPECT(seen.Image == entry.Image);
		CDL_EXPECT_NE(seen.BindlessIndex, std::numeric_limits<uint32_t>::max());
		CDL_EXPECT_EQ(seen.BindlessIndex, entry.BindlessIndex);
	}
	ExpectNoVulkanDebugMessages(logs);
}
