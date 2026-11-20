// FrameGraph's setup half. AddPass runs setup at once and hands back the data it wrote, out of an arena Reset frees.
// Imports record the states the graph finds their images in and must leave them in. The graph needs a real TexturePool,
// so these are Gpu tests.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "../RenderTestHelpers.h"

#include <Candle/Render/FrameGraph/FrameGraph.h>
#include <Candle/Render/Resources/TexturePool.h>
#include <Candle/Render/Vulkan/SwapChainImage.h>

#include <limits>
#include <utility>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Gpu;

namespace {

	struct TestFrameGraph : TestTextures
	{
		FrameGraph Graph{ { .Allocator = Allocator, .BindlessTable = Bindless, .TexturePool = Pool } };
	};

	struct ValueData
	{
		uint32_t Value{ 0 };
	};

	constexpr auto kNoSetup = [](FrameGraphBuilder&, auto&) {};
	constexpr auto kNoExecute = [](const auto&, FrameGraphContext&) {};

	constexpr vk::Format kFormat = vk::Format::eR8G8B8A8Unorm;

	// Where SwapChain::AquireNextImage leaves an image
	constexpr ImageState kAcquiredState{
		.Layout = vk::ImageLayout::eUndefined,
		.Access = vk::AccessFlagBits2::eNone,
		.Stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput };

	// Anything outside the graph may sample a pool texture in any of these, so the import's states must cover all of them
	bool CoversEverySampledStage(vk::PipelineStageFlags2 stages)
	{
		const vk::PipelineStageFlags2 sampled = vk::PipelineStageFlagBits2::eVertexShader | vk::PipelineStageFlagBits2::eFragmentShader
			| vk::PipelineStageFlagBits2::eComputeShader;
		return (stages & sampled) == sampled;
	}

	void ExpectImportOf(const FGTextureEntry& entry, const TextureInfo& info)
	{
		CDL_EXPECT(entry.Imported);
		CDL_EXPECT(entry.Image == info.Image);
		CDL_EXPECT(entry.ImageView == info.ImageView);
		CDL_EXPECT(entry.Desc.Extent == info.Desc.Extent);
		CDL_EXPECT_EQ(entry.Desc.Format, info.Desc.Format);
		CDL_EXPECT(entry.Desc.Usage == info.Desc.Usage);
		CDL_EXPECT_EQ(entry.BindlessIndex, info.Index);

		CDL_EXPECT_EQ(entry.InitialState.Layout, info.SampledLayout);
		CDL_EXPECT(entry.InitialState.Access == vk::AccessFlags2{});	// Earlier reads only need to finish before a write
		CDL_EXPECT(CoversEverySampledStage(entry.InitialState.Stage));

		CDL_EXPECT_EQ(entry.FinalState.Layout, info.SampledLayout);
		CDL_EXPECT(entry.FinalState.Access == vk::AccessFlags2{ vk::AccessFlagBits2::eShaderSampledRead });
		CDL_EXPECT(CoversEverySampledStage(entry.FinalState.Stage));
	}

}

CDL_TEST_CASE(FrameGraph, AddPassRunsSetupAtOnceAndReturnsItsData, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		test.Graph.Reset({ 4, 4 });

		bool setupRan = false;
		const ValueData& data = test.Graph.AddRasterPass<ValueData>("Pass",
			[&setupRan](FrameGraphBuilder&, ValueData& passData) { setupRan = true; passData.Value = 7; }, kNoExecute);

		CDL_EXPECT(setupRan);
		CDL_EXPECT_EQ(data.Value, 7u);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// m_Passes reallocates as it grows, but pass data lives in the arena, so an early pass's data stays where it was
CDL_TEST_CASE(FrameGraph, PassDataStaysValidAsMorePassesAreAdded, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		test.Graph.Reset({ 4, 4 });

		const ValueData& first = test.Graph.AddRasterPass<ValueData>("First",
			[](FrameGraphBuilder&, ValueData& passData) { passData.Value = 1; }, kNoExecute);
		for (uint32_t i = 0; i < 100; ++i)
		{
			test.Graph.AddRasterPass<ValueData>("Later",
				[i](FrameGraphBuilder&, ValueData& passData) { passData.Value = i + 2; }, kNoExecute);
		}

		CDL_EXPECT_EQ(first.Value, 1u);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Otherwise the arena fills a little more every frame until it runs out, and handles keep counting up from last frame's
CDL_TEST_CASE(FrameGraph, ResetFreesTheArenaAndTheFramesTextures, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		const GPUTextureHandle first = test.Pool.CreateRenderTarget({ .Extent = { 4, 4 }, .Format = kFormat, .DebugName = "First Target" });
		const GPUTextureHandle second = test.Pool.CreateRenderTarget({ .Extent = { 4, 4 }, .Format = kFormat, .DebugName = "Second Target" });

		test.Graph.Reset({ 4, 4 });
		const ValueData* lastFrame = &test.Graph.AddRasterPass<ValueData>("Pass", kNoSetup, kNoExecute);
		test.Graph.ImportTexture("First", first);
		test.Graph.ImportTexture("Second", second);

		test.Graph.Reset({ 4, 4 });
		const ValueData* thisFrame = &test.Graph.AddRasterPass<ValueData>("Pass", kNoSetup, kNoExecute);
		const FGTextureHandle imported = test.Graph.ImportTexture("Second", second);

		CDL_EXPECT(thisFrame == lastFrame);
		CDL_EXPECT_EQ(imported.Index, 0u);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The pool's descriptors expect SampledLayout, so the graph must find each import there and leave it there. A storage
// target's is General, which catches a layout hard-coded to ShaderReadOnlyOptimal.
CDL_TEST_CASE(FrameGraph, ImportedTextureStartsAndEndsInItsSampledLayout, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		const GPUTextureHandle sampled = test.Pool.CreateRenderTarget({ .Extent = { 8, 4 }, .Format = kFormat, .DebugName = "Sampled Target" });
		const GPUTextureHandle storage = test.Pool.CreateRenderTarget({ .Extent = { 8, 4 }, .Format = kFormat,
			.Usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled, .DebugName = "Storage Target" });
		CDL_CHECK_EQ(test.Pool.GetTextureInfo(storage).SampledLayout, vk::ImageLayout::eGeneral);

		test.Graph.Reset({ 8, 4 });
		const FGTextureHandle sampledHandle = test.Graph.ImportTexture("Sampled", sampled);
		const FGTextureHandle storageHandle = test.Graph.ImportTexture("Storage", storage);

		ExpectImportOf(test.Graph.GetTexture(sampledHandle), test.Pool.GetTextureInfo(sampled));
		ExpectImportOf(test.Graph.GetTexture(storageHandle), test.Pool.GetTextureInfo(storage));
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The first barrier has to chain with the acquire semaphore's wait, so the graph starts from whatever acquire left, and it
// must leave the image ready to present
CDL_TEST_CASE(FrameGraph, ImportedSwapChainImageStartsWhereAcquireLeftItAndEndsReadyToPresent, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		OwnedImage owned = CreateOwnedImage(test.Device, kFormat, { 4, 4 }, 1, vk::ImageUsageFlagBits::eColorAttachment);
		vk::raii::ImageView view(test.Device.GetDevice(), vk::ImageViewCreateInfo{
			.image = *owned.Handle, .viewType = vk::ImageViewType::e2D, .format = kFormat, .subresourceRange = SwapChainImage::kSubresourceRange });
		SwapChainImage image(*owned.Handle, std::move(view), ImageDesc{ .Extent = { 4, 4 }, .Format = kFormat, .Usage = vk::ImageUsageFlagBits::eColorAttachment });
		image.SetState(kAcquiredState);

		test.Graph.Reset({ 4, 4 });
		const FGTextureEntry& entry = test.Graph.GetTexture(test.Graph.ImportSwapChainImage(image));

		CDL_EXPECT(entry.Imported);
		CDL_EXPECT(entry.Image == *owned.Handle);
		CDL_EXPECT(entry.ImageView == image.GetImageView());
		CDL_EXPECT(entry.Desc.Extent == glm::uvec2(4, 4));
		CDL_EXPECT(entry.Desc.Usage == vk::ImageUsageFlags{ vk::ImageUsageFlagBits::eColorAttachment });
		CDL_EXPECT_EQ(entry.BindlessIndex, std::numeric_limits<uint32_t>::max());

		CDL_EXPECT_EQ(entry.InitialState.Layout, kAcquiredState.Layout);
		CDL_EXPECT(entry.InitialState.Stage == kAcquiredState.Stage);
		CDL_EXPECT_EQ(entry.FinalState.Layout, vk::ImageLayout::ePresentSrcKHR);
	}
	ExpectNoVulkanDebugMessages(logs);
}
