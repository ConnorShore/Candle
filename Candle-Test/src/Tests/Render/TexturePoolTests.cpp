// TexturePool on a real device: texture and render target creation, their info, slot reuse through the free list, the
// image a released texture gives back once its frame retires, and what a shader samples from it. Each test fails on any
// validation message.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "RenderTestHelpers.h"

#include <Candle/Render/Resources/GPUHandles.h>
#include <Candle/Render/Vulkan/SamplerType.h>
#include <Candle/Render/Assets/TextureData.h>
#include <Candle/Render/Resources/TexturePool.h>
#include <Candle/Render/Vulkan/VulkanUtils.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <span>
#include <vector>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Gpu;

namespace {

	// One texel is enough for the tests that only care about slots and lifetimes.
	GPUTextureHandle CreateTexel(TexturePool& pool)
	{
		return pool.CreateTexture({
			.Data = std::as_bytes(std::span(&kWhite, 1)), .Extent = { 1, 1 }, .Format = TextureFormat::Rgba8, .ColorSpace = TextureColorSpace::Linear });
	}

	// UNORM, so a cleared value is stored and sampled back unconverted
	constexpr vk::Format kRenderTargetFormat = vk::Format::eR8G8B8A8Unorm;

	GPUTextureHandle CreateRenderTarget(TexturePool& pool, glm::uvec2 extent)
	{
		return pool.CreateRenderTarget({ .Extent = extent, .Format = kRenderTargetFormat, .DebugName = "Test Render Target" });
	}

	constexpr ImageState kShaderReadState{
		.Layout = vk::ImageLayout::eShaderReadOnlyOptimal,
		.Access = vk::AccessFlagBits2::eShaderSampledRead,
		.Stage = vk::PipelineStageFlagBits2::eFragmentShader };

	// Draws into the target as the scene pass does, a clear standing in for the draws, and leaves it where Renderer does:
	// ready for a shader to sample through its bindless slot.
	void ClearRenderTarget(TestTextures& gpu, GPUTextureHandle target, const Texel& colour)
	{
		const TextureInfo info = gpu.Pool.GetTextureInfo(target);
		TestCommands commands(gpu.Device);
		commands.Buffer.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

		const vk::ImageMemoryBarrier2 toAttachment = MakeImageBarrier(info.Image, {}, kColorAttachmentState, kColorSubresourceRange);
		commands.Buffer.pipelineBarrier2({ .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &toAttachment });

		const vk::RenderingAttachmentInfo attachment{
			.imageView = info.ImageView, .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear, .storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = vk::ClearColorValue(colour[0] / 255.0f, colour[1] / 255.0f, colour[2] / 255.0f, colour[3] / 255.0f) };
		commands.Buffer.beginRendering({ .renderArea = { { 0, 0 }, { info.Extent.x, info.Extent.y } }, .layerCount = 1,
			.colorAttachmentCount = 1, .pColorAttachments = &attachment });
		commands.Buffer.endRendering();

		const vk::ImageMemoryBarrier2 toSampled = MakeImageBarrier(info.Image, kColorAttachmentState, kShaderReadState, kColorSubresourceRange);
		commands.Buffer.pipelineBarrier2({ .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &toSampled });
		commands.Buffer.end();
		SubmitAndWait(gpu.Device, commands.Buffer);
	}

	// Every format and colour space ToVulkanFormat accepts; R8 and RG8 are linear only.
	struct FormatCase
	{
		TextureFormat Format;
		TextureColorSpace ColorSpace;
		const char* Name;
	};

	constexpr FormatCase kFormatCases[] = {
		{ TextureFormat::Rgba8, TextureColorSpace::Srgb, "RGBA8 sRGB" },
		{ TextureFormat::Rgba8, TextureColorSpace::Linear, "RGBA8 linear" },
		{ TextureFormat::Rg8, TextureColorSpace::Linear, "RG8" },
		{ TextureFormat::R8, TextureColorSpace::Linear, "R8" } };

}

CDL_TEST_CASE(TexturePool, CreateTextureReturnsAValidHandleAndItsInfo, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestTextures gpu;
		const GPUTextureHandle texture = CreateTexel(gpu.Pool);
		const TextureInfo info = gpu.Pool.GetTextureInfo(texture);

		CDL_EXPECT(texture.IsValid());
		CDL_EXPECT(gpu.Pool.IsAlive(texture));
		CDL_EXPECT_EQ(info.Index, texture.Index);	// The bindless element the texture is published at
		CDL_EXPECT(info.ImageView);
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), kDefaultTextureAllocations + 2u);	// The image and its upload's staging

		RunReleaseFrame(gpu);
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), kDefaultTextureAllocations + 1u);
	}
	ExpectNoVulkanDebugMessages(logs);
}

CDL_TEST_CASE(TexturePool, LiveTexturesGetDistinctSlotsAndViews, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestTextures gpu;
		const GPUTextureHandle first = CreateTexel(gpu.Pool);
		const GPUTextureHandle second = CreateTexel(gpu.Pool);

		CDL_EXPECT_NE(first.Index, second.Index);
		CDL_EXPECT(gpu.Pool.GetTextureInfo(first).ImageView != gpu.Pool.GetTextureInfo(second).ImageView);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Not square, so swapped extents show. Only render targets are drawn into, and the renderer asserts on that.
CDL_TEST_CASE(TexturePool, TextureInfoReportsItsExtentAndThatItIsNoRenderTarget, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestTextures gpu;
		const std::vector<Texel> texels(3 * 2, kWhite);
		const TextureInfo info = gpu.Pool.GetTextureInfo(CreateBoundTexture(gpu, { 3, 2 }, texels));

		CDL_EXPECT(info.Extent == glm::uvec2(3, 2));
		CDL_EXPECT(info.Image);
		CDL_EXPECT_FALSE(info.IsRenderTarget);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Nothing is uploaded, so the target costs its image and no staging buffer.
CDL_TEST_CASE(TexturePool, CreateRenderTargetReturnsAValidHandleAndItsInfo, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestTextures gpu;
		const GPUTextureHandle target = CreateRenderTarget(gpu.Pool, { 5, 3 });
		const TextureInfo info = gpu.Pool.GetTextureInfo(target);

		CDL_EXPECT(target.IsValid());
		CDL_EXPECT(gpu.Pool.IsAlive(target));
		CDL_EXPECT_EQ(info.Index, target.Index);	// The bindless element the target is published at, for the GUI to sample
		CDL_EXPECT(info.ImageView);
		CDL_EXPECT(info.Image);
		CDL_EXPECT(info.Extent == glm::uvec2(5, 3));
		CDL_EXPECT(info.IsRenderTarget);
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), kDefaultTextureAllocations + 1u);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Render targets share the textures' slots, generations and bindless elements, so one never aliases the other.
CDL_TEST_CASE(TexturePool, RenderTargetsAndTexturesGetDistinctSlots, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestTextures gpu;
		const GPUTextureHandle texture = CreateTexel(gpu.Pool);
		const GPUTextureHandle target = CreateRenderTarget(gpu.Pool, { 1, 1 });

		CDL_EXPECT_NE(texture.Index, target.Index);
		CDL_EXPECT(gpu.Pool.GetTextureInfo(texture).ImageView != gpu.Pool.GetTextureInfo(target).ImageView);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// RenderContext releases a target whenever the output size changes, while frames that drew into it may still be in flight.
CDL_TEST_CASE(TexturePool, ReleasedRenderTargetFreesItsImageOnceTheFrameRetires, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestTextures gpu;
		const GPUTextureHandle target = CreateRenderTarget(gpu.Pool, { 4, 4 });
		CDL_CHECK_EQ(LiveAllocations(gpu.Allocator), kDefaultTextureAllocations + 1u);

		gpu.Pool.ReleaseTexture(target);
		CDL_EXPECT_FALSE(gpu.Pool.IsAlive(target));
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), kDefaultTextureAllocations + 1u);

		RunReleaseFrame(gpu);
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), kDefaultTextureAllocations);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The scene draws into the target and the GUI samples it: the image needs both usages, and its view must be published at
// the handle's slot. Validation catches a missing usage, and the colour a view at the wrong slot.
CDL_TEST_CASE(TexturePool, RenderTargetCanBeDrawnIntoThenSampled, Gpu)
{
	SkipUnlessCompiled(kBindlessFixturePath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestTextures gpu;
		constexpr glm::uvec2 kExtent{ 3, 2 };
		constexpr Texel kClearColour{ 51, 102, 153, 255 };
		const GPUTextureHandle target = CreateRenderTarget(gpu.Pool, kExtent);

		ClearRenderTarget(gpu, target, kClearColour);
		const std::vector<Texel> sampled = SampleTextures(gpu, ProbeEveryTexel(target, kExtent));

		CDL_CHECK_EQ(sampled.size(), size_t{ kExtent.x } * kExtent.y);
		for (size_t i = 0; i < sampled.size(); ++i)
			ExpectTexelNear(sampled[i], kClearColour, std::format("texel {}", i), 0);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// A frame still in flight may sample a released texture, so its image outlives the release until that frame retires.
CDL_TEST_CASE(TexturePool, ReleaseFreesTheImageOnlyOnceTheFrameRetires, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestTextures gpu;
		const GPUTextureHandle texture = CreateTexel(gpu.Pool);
		RunReleaseFrame(gpu);
		CDL_CHECK_EQ(LiveAllocations(gpu.Allocator), kDefaultTextureAllocations + 1u);

		gpu.Pool.ReleaseTexture(texture);
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), kDefaultTextureAllocations + 1u);

		RunReleaseFrame(gpu);
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), kDefaultTextureAllocations);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The generation is what tells a stale handle from the texture that reused its slot, and so its bindless element.
CDL_TEST_CASE(TexturePool, ReusesAReleasedSlotWithTheNextGeneration, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestTextures gpu;
		const GPUTextureHandle released = CreateTexel(gpu.Pool);
		gpu.Pool.ReleaseTexture(released);
		RunReleaseFrame(gpu);

		const GPUTextureHandle reused = CreateTexel(gpu.Pool);

		CDL_EXPECT_EQ(reused.Index, released.Index);
		CDL_EXPECT_EQ(reused.Generation, released.Generation + 1);
		CDL_EXPECT_EQ(gpu.Pool.GetTextureInfo(reused).Index, released.Index);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Until the render thread hands the slot back, a frame may still sample its element, so a new texture takes another one.
CDL_TEST_CASE(TexturePool, ReleasedSlotIsNotReusedBeforeItRetires, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestTextures gpu;
		const GPUTextureHandle released = CreateTexel(gpu.Pool);
		gpu.Pool.ReleaseTexture(released);

		const GPUTextureHandle created = CreateTexel(gpu.Pool);
		CDL_EXPECT_NE(created.Index, released.Index);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// A handle dies when its texture is released, before the GPU is done with it, and stays dead once another takes its slot.
CDL_TEST_CASE(TexturePool, HandlesDieWithTheirTexture, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestTextures gpu;
		const GPUTextureHandle texture = CreateTexel(gpu.Pool);
		CDL_EXPECT(gpu.Pool.IsAlive(texture));

		gpu.Pool.ReleaseTexture(texture);
		CDL_EXPECT_FALSE(gpu.Pool.IsAlive(texture));

		RunReleaseFrame(gpu);
		const GPUTextureHandle reused = CreateTexel(gpu.Pool);
		CDL_CHECK_EQ(reused.Index, texture.Index);
		CDL_EXPECT(gpu.Pool.IsAlive(reused));
		CDL_EXPECT_FALSE(gpu.Pool.IsAlive(texture));
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The default handle, a slot past the end, and a generation the slot hasn't reached all miss.
CDL_TEST_CASE(TexturePool, HandlesItNeverIssuedAreNotAlive, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestTextures gpu;
		const GPUTextureHandle texture = CreateTexel(gpu.Pool);

		CDL_EXPECT_FALSE(gpu.Pool.IsAlive(GPUTextureHandle{}));
		CDL_EXPECT_FALSE(gpu.Pool.IsAlive(GPUTextureHandle{ .Index = texture.Index + 1, .Generation = texture.Generation }));
		CDL_EXPECT_FALSE(gpu.Pool.IsAlive(GPUTextureHandle{ .Index = texture.Index, .Generation = texture.Generation + 1 }));
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Every texel of a non-square texture, read at its centre with no filtering, so swapped extents or rows show up. The
// caller's bytes are overwritten straight after CreateTexture, which its contract allows, before the GPU has run anything.
CDL_TEST_CASE(TexturePool, SamplesBackTheTexelsItWasGiven, Gpu)
{
	SkipUnlessCompiled(kBindlessFixturePath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestTextures gpu;
		constexpr glm::uvec2 kExtent{ 3, 2 };
		const std::vector<Texel> expected = {
			{ 10, 20, 30, 255 }, { 40, 50, 60, 255 }, { 70, 80, 90, 255 },
			{ 100, 110, 120, 255 }, { 130, 140, 150, 255 }, { 160, 170, 180, 0 } };

		std::vector<Texel> texels = expected;
		const GPUTextureHandle texture = CreateBoundTexture(gpu, kExtent, texels);
		std::ranges::fill(texels, Texel{});

		const std::vector<Texel> sampled = SampleTextures(gpu, ProbeEveryTexel(texture, kExtent));
		CDL_CHECK_EQ(sampled.size(), expected.size());
		for (size_t i = 0; i < sampled.size(); ++i)
			ExpectTexelNear(sampled[i], expected[i], std::format("texel {}", i), 0);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// sRGB 188 is linear ~0.503: an sRGB texture is decoded when sampled, a linear one returns its bytes as they are.
// Alpha is never sRGB-encoded, so it stays put in both.
CDL_TEST_CASE(TexturePool, ColorSpacePicksHowSamplesAreDecoded, Gpu)
{
	SkipUnlessCompiled(kBindlessFixturePath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestTextures gpu;
		constexpr Texel kGrey{ 188, 188, 188, 188 };
		const GPUTextureHandle srgb = CreateBoundTexture(gpu, { 1, 1 }, std::array<Texel, 1>{ kGrey }, TextureColorSpace::Srgb);
		const GPUTextureHandle linear = CreateBoundTexture(gpu, { 1, 1 }, std::array<Texel, 1>{ kGrey }, TextureColorSpace::Linear);

		const std::array probes = {
			TextureProbe{ .Texture = srgb, .Sampler = SamplerType::PointClamp, .UV = { 0.5f, 0.5f } },
			TextureProbe{ .Texture = linear, .Sampler = SamplerType::PointClamp, .UV = { 0.5f, 0.5f } } };
		const std::vector<Texel> sampled = SampleTextures(gpu, probes);

		CDL_CHECK_EQ(sampled.size(), probes.size());
		ExpectTexelNear(sampled[0], Texel{ 128, 128, 128, 188 }, "sRGB");
		ExpectTexelNear(sampled[1], kGrey, "linear");
	}
	ExpectNoVulkanDebugMessages(logs);
}

// What TexturePool asks of an image: optimally tiled, a copy destination, and sampled with linear filtering, as the
// bindless samplers do. The spec guarantees all three for every format ToVulkanFormat returns; this catches one that strays.
CDL_TEST_CASE(TexturePool, EveryTextureFormatIsSampleableOnThisDevice, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		constexpr vk::FormatFeatureFlags kNeeded =
			vk::FormatFeatureFlagBits::eSampledImage | vk::FormatFeatureFlagBits::eSampledImageFilterLinear | vk::FormatFeatureFlagBits::eTransferDst;

		for (const FormatCase& c : kFormatCases)
		{
			const vk::Format format = ToVulkanFormat(c.Format, c.ColorSpace);
			const vk::FormatFeatureFlags features = gpu.Device.GetPhysicalDevice().getFormatProperties(format).optimalTilingFeatures;
			CDL_EXPECT_MSG((features & kNeeded) == kNeeded, std::format("{} ({})", c.Name, vk::to_string(format)));
		}
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Rows of 3 and 6 bytes, not multiples of 4, so the data must stay tightly packed from CreateTexture to the image.
// A shader reads the channels a format lacks as 0, and its missing alpha as 1.
CDL_TEST_CASE(TexturePool, SamplesBackSingleAndDualChannelTexels, Gpu)
{
	SkipUnlessCompiled(kBindlessFixturePath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestTextures gpu;
		constexpr glm::uvec2 kExtent{ 3, 2 };
		constexpr size_t kTexels = kExtent.x * kExtent.y;
		const std::vector<std::byte> red = BytePattern(kTexels, 50);
		const std::vector<std::byte> redGreen = BytePattern(kTexels * 2, 100);
		const GPUTextureHandle r8 = CreateBoundTexture(gpu, {
			.Data = red, .Extent = kExtent, .Format = TextureFormat::R8, .ColorSpace = TextureColorSpace::Linear });
		const GPUTextureHandle rg8 = CreateBoundTexture(gpu, {
			.Data = redGreen, .Extent = kExtent, .Format = TextureFormat::Rg8, .ColorSpace = TextureColorSpace::Linear });

		std::vector<TextureProbe> probes = ProbeEveryTexel(r8, kExtent);
		const std::vector<TextureProbe> rg8Probes = ProbeEveryTexel(rg8, kExtent);
		probes.insert(probes.end(), rg8Probes.begin(), rg8Probes.end());
		const std::vector<Texel> sampled = SampleTextures(gpu, probes);

		CDL_CHECK_EQ(sampled.size(), 2 * kTexels);
		for (size_t i = 0; i < kTexels; ++i)
		{
			const Texel expectedR8{ static_cast<uint8_t>(50 + i), 0, 0, 255 };
			const Texel expectedRG8{ static_cast<uint8_t>(100 + 2 * i), static_cast<uint8_t>(101 + 2 * i), 0, 255 };
			ExpectTexelNear(sampled[i], expectedR8, std::format("R8 texel {}", i), 0);
			ExpectTexelNear(sampled[kTexels + i], expectedRG8, std::format("RG8 texel {}", i), 0);
		}
	}
	ExpectNoVulkanDebugMessages(logs);
}
