// Texture before any handler has seen it: its type, where it loads from, how it is sampled, and no GPU texture yet.

#include "TestFramework.h"

#include <Candle/Asset/Asset.h>
#include <Candle/Render/Assets/Texture.h>
#include <Candle/Render/SamplerType.h>
#include <Candle/Render/TextureData.h>

#include <filesystem>

using namespace Candle;
using Candle::Test::Type::Unit;

CDL_STATIC_ASSERT(IsAsset<Texture>);

namespace {

	constexpr const char* kTextureName = "Box";
	const std::filesystem::path kTexturePath = "Candle/res/textures/box.jpg";

}

CDL_TEST_CASE(Texture, IsATextureAsset, Unit)
{
	const Texture texture(kTextureName, kTexturePath);

	CDL_EXPECT_EQ(Texture::kType, AssetType::Texture);
	CDL_EXPECT_EQ(texture.GetType(), AssetType::Texture);
}

// The handler loads from the path, so it must survive as given; the name is what AssetManager keys it by.
CDL_TEST_CASE(Texture, KeepsItsNameAndPathAndStartsWithoutAGPUTexture, Unit)
{
	const Texture texture(kTextureName, kTexturePath);

	CDL_EXPECT_EQ(texture.GetName(), std::string(kTextureName));
	CDL_EXPECT(texture.GetFilePath() == kTexturePath);
	CDL_EXPECT_FALSE(texture.GetGPUHandle().IsValid());
}

// AssetManager::LoadAsset<Texture>(name, path) takes this constructor, so its defaults are what a plain load samples with.
CDL_TEST_CASE(Texture, WithoutTextureDataItIsAnSrgbLinearWrapTexture, Unit)
{
	const Texture texture(kTextureName, kTexturePath);

	CDL_EXPECT_EQ(texture.GetTextureData().ColorSpace, TextureColorSpace::Srgb);
	CDL_EXPECT_EQ(texture.GetTextureData().Sampler, SamplerType::LinearWrap);
}

CDL_TEST_CASE(Texture, KeepsTheTextureDataItIsGiven, Unit)
{
	const Texture texture(kTextureName, kTexturePath, TextureData{ .Sampler = SamplerType::PointClamp, .ColorSpace = TextureColorSpace::Linear });

	CDL_EXPECT_EQ(texture.GetTextureData().Sampler, SamplerType::PointClamp);
	CDL_EXPECT_EQ(texture.GetTextureData().ColorSpace, TextureColorSpace::Linear);
}

// TexturePool sizes the data it accepts from this, so a wrong entry fails its assert on every texture of that format.
CDL_TEST_CASE(TextureData, BytesPerTexelCountsEachFormatsChannels, Unit)
{
	CDL_EXPECT_EQ(BytesPerTexel(TextureFormat::Rgba8), 4u);
	CDL_EXPECT_EQ(BytesPerTexel(TextureFormat::Rg8), 2u);
	CDL_EXPECT_EQ(BytesPerTexel(TextureFormat::R8), 1u);
}
