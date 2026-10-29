// TextureHandler loading files the test writes into a real TexturePool and BindlessTable, called directly and through
// AssetManager. Each test fails on any validation message.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "../RenderTestHelpers.h"

#include <Candle/Asset/AssetManager.h>
#include <Candle/Render/Assets/Texture.h>
#include <Candle/Render/Assets/TextureHandler.h>
#include <Candle/Render/SamplerType.h>
#include <Candle/Render/TextureData.h>

#include <array>
#include <filesystem>
#include <vector>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Gpu;

namespace {

	SharedPtr<Texture> CreateTextureAsset(const std::filesystem::path& path, TextureColorSpace colorSpace = TextureColorSpace::Srgb)
	{
		return SharedPtr<Texture>::Create(path.stem().string(), path, TextureData{ .Sampler = SamplerType::PointClamp, .ColorSpace = colorSpace });
	}

	TextureProbe ProbeCentre(const Texture& texture)
	{
		return { .Texture = texture.GetGPUHandle(), .Sampler = texture.GetTextureData().Sampler, .UV = { 0.5f, 0.5f } };
	}

}

CDL_TEST_CASE(TextureHandler, LoadUploadsTheFileAndStoresItsHandle, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TempDirectory dir;
		TestTextures gpu;
		TextureHandler handler(gpu.Pool);
		IAssetHandler& untyped = handler;
		SharedPtr<Texture> asset = CreateTextureAsset(WritePPM(dir, "White.ppm", { 1, 1 }, std::array<Texel, 1>{ kWhite }));

		CDL_EXPECT_EQ(untyped.GetAssetType(), AssetType::Texture);
		untyped.OnLoad(*asset);

		CDL_CHECK(asset->GetGPUHandle().IsValid());
		CDL_EXPECT(gpu.Pool.IsAlive(asset->GetGPUHandle()));

		RunReleaseFrame(gpu);
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), kDefaultTextureAllocations + 1u);	// The image, once the upload's staging is drained
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The file decides the extent and format, so the handler records them; how to sample it stays as the asset was given.
CDL_TEST_CASE(TextureHandler, LoadFillsInTheExtentAndFormatFromTheFile, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TempDirectory dir;
		TestTextures gpu;
		TextureHandler handler(gpu.Pool);
		IAssetHandler& untyped = handler;
		const std::array<Texel, 6> texels = { kRed, kGreen, kBlue, kWhite, kRed, kGreen };
		const std::filesystem::path path = WritePPM(dir, "Wide.ppm", { 3, 2 }, texels);

		// Starts with a format and extent the file disagrees with, since Rgba8 and 0 x 0 are what TextureData defaults to
		SharedPtr<Texture> asset = SharedPtr<Texture>::Create("Wide", path, TextureData{
			.Sampler = SamplerType::PointClamp, .ColorSpace = TextureColorSpace::Linear, .Format = TextureFormat::R8, .Extent = { 1, 1 } });

		untyped.OnLoad(*asset);

		const TextureData& data = asset->GetTextureData();
		CDL_EXPECT(data.Extent == glm::uvec2(3, 2));
		CDL_EXPECT_EQ(data.Format, TextureFormat::Rgba8);	// The loader expands RGB, so a PPM comes back as RGBA8
		CDL_EXPECT_EQ(data.Sampler, SamplerType::PointClamp);
		CDL_EXPECT_EQ(data.ColorSpace, TextureColorSpace::Linear);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The handler, not the pool, publishes the texture, so a shader can only see it if the handler wrote its own slot.
CDL_TEST_CASE(TextureHandler, LoadPublishesTheTextureAtItsSlotOfTheBindlessTable, Gpu)
{
	SkipUnlessCompiled(kBindlessFixturePath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TempDirectory dir;
		TestTextures gpu;
		TextureHandler handler(gpu.Pool);
		IAssetHandler& untyped = handler;
		SharedPtr<Texture> red = CreateTextureAsset(WritePPM(dir, "Red.ppm", { 1, 1 }, std::array<Texel, 1>{ kRed }));
		SharedPtr<Texture> green = CreateTextureAsset(WritePPM(dir, "Green.ppm", { 1, 1 }, std::array<Texel, 1>{ kGreen }));
		untyped.OnLoad(*red);
		untyped.OnLoad(*green);
		CDL_CHECK(red->GetGPUHandle().IsValid() && green->GetGPUHandle().IsValid());

		const std::vector<Texel> sampled = SampleTextures(gpu, std::array{ ProbeCentre(*red), ProbeCentre(*green) });

		CDL_CHECK_EQ(sampled.size(), 2u);
		ExpectTexelNear(sampled[0], kRed, "red", 0);
		ExpectTexelNear(sampled[1], kGreen, "green", 0);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The file carries no colour space, so the asset's TextureData decides it: sRGB 188 samples as linear ~128.
CDL_TEST_CASE(TextureHandler, LoadTakesTheColorSpaceFromTheAsset, Gpu)
{
	SkipUnlessCompiled(kBindlessFixturePath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TempDirectory dir;
		TestTextures gpu;
		TextureHandler handler(gpu.Pool);
		IAssetHandler& untyped = handler;
		const std::filesystem::path path = WritePPM(dir, "Grey.ppm", { 1, 1 }, std::array<Texel, 1>{ Texel{ 188, 188, 188, 255 } });
		SharedPtr<Texture> srgb = CreateTextureAsset(path, TextureColorSpace::Srgb);
		SharedPtr<Texture> linear = CreateTextureAsset(path, TextureColorSpace::Linear);
		untyped.OnLoad(*srgb);
		untyped.OnLoad(*linear);
		CDL_CHECK(srgb->GetGPUHandle().IsValid() && linear->GetGPUHandle().IsValid());

		const std::vector<Texel> sampled = SampleTextures(gpu, std::array{ ProbeCentre(*srgb), ProbeCentre(*linear) });

		CDL_CHECK_EQ(sampled.size(), 2u);
		ExpectTexelNear(sampled[0], Texel{ 128, 128, 128, 255 }, "sRGB");
		ExpectTexelNear(sampled[1], Texel{ 188, 188, 188, 255 }, "linear");
	}
	ExpectNoVulkanDebugMessages(logs);
}

// A file that won't load leaves the asset without a texture rather than half-creating one.
CDL_TEST_CASE(TextureHandler, LoadOfAMissingFileLeavesNoTexture, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TempDirectory dir;
		TestTextures gpu;
		TextureHandler handler(gpu.Pool);
		IAssetHandler& untyped = handler;
		SharedPtr<Texture> asset = CreateTextureAsset(dir / "Missing.ppm");

		untyped.OnLoad(*asset);

		CDL_EXPECT_FALSE(asset->GetGPUHandle().IsValid());
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), kDefaultTextureAllocations);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The handle is cleared at once; the image goes when the frame that carried the release retires.
CDL_TEST_CASE(TextureHandler, UnloadReleasesTheTextureAndClearsItsHandle, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TempDirectory dir;
		TestTextures gpu;
		TextureHandler handler(gpu.Pool);
		IAssetHandler& untyped = handler;
		SharedPtr<Texture> asset = CreateTextureAsset(WritePPM(dir, "White.ppm", { 1, 1 }, std::array<Texel, 1>{ kWhite }));
		untyped.OnLoad(*asset);
		const GPUTextureHandle texture = asset->GetGPUHandle();

		untyped.OnUnload(*asset);

		CDL_EXPECT_FALSE(asset->GetGPUHandle().IsValid());
		CDL_EXPECT_FALSE(gpu.Pool.IsAlive(texture));

		RunReleaseFrame(gpu);
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), kDefaultTextureAllocations);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Application's path: LoadAsset<Texture>(name, path) builds the asset itself, so it loads with Texture's defaults.
CDL_TEST_CASE(TextureHandler, AssetManagerLoadsTexturesFromAPathThroughIt, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TempDirectory dir;
		TestTextures gpu;
		AssetManager assets;	// After the pool its handler uploads into, so it is destroyed first
		assets.RegisterAssetHandler(ScopedPtr<TextureHandler>::Create(gpu.Pool));

		const uint64_t uuid = assets.LoadAsset<Texture>("Box", WritePPM(dir, "Box.ppm", { 1, 1 }, std::array<Texel, 1>{ kWhite }));
		const SharedPtr<Texture> asset = assets.GetAsset<Texture>("Box");

		CDL_CHECK(asset != nullptr);
		CDL_EXPECT(assets.GetAsset(uuid).Get() == asset.Get());
		CDL_EXPECT(asset->GetGPUHandle().IsValid());
		CDL_EXPECT(gpu.Pool.IsAlive(asset->GetGPUHandle()));
	}
	ExpectNoVulkanDebugMessages(logs);
}

CDL_TEST_CASE(TextureHandler, AssetManagerUnloadsTexturesThroughIt, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TempDirectory dir;
		TestTextures gpu;
		AssetManager assets;
		assets.RegisterAssetHandler(ScopedPtr<TextureHandler>::Create(gpu.Pool));
		SharedPtr<Texture> asset = CreateTextureAsset(WritePPM(dir, "White.ppm", { 1, 1 }, std::array<Texel, 1>{ kWhite }));
		assets.LoadAsset(asset);

		assets.UnloadAsset("White", AssetType::Texture);

		CDL_EXPECT_FALSE(asset->GetGPUHandle().IsValid());
		CDL_EXPECT(assets.GetAsset<Texture>("White") == nullptr);

		RunReleaseFrame(gpu);
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), kDefaultTextureAllocations);
	}
	ExpectNoVulkanDebugMessages(logs);
}
