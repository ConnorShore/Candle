// TextureLoader decoding files written by the test: every texel comes back RGBA8 in the file's own row order, and a file
// it can't read or decode is an error rather than a texture. No GPU, so Unit.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "RenderTestHelpers.h"

#include <Candle/Platform/Platform.h>
#include <Candle/Render/TextureLoader.h>

#include <array>
#include <cstddef>
#include <span>
#include <string>
#include <vector>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Unit;

// TexturePool only takes four bytes a texel, so an RGB file must come back with an opaque alpha added.
CDL_TEST_CASE(TextureLoader, DecodesRgbFilesToOpaqueRgba, Unit)
{
	TempDirectory dir;
	const std::array<Texel, 2> texels = { Texel{ 10, 20, 30, 0 }, Texel{ 40, 50, 60, 0 } };	// The PPM drops these alphas

	const auto loaded = TextureLoader::LoadTexture(WritePPM(dir, "Pair.ppm", { 2, 1 }, texels));

	CDL_CHECK_MSG(loaded.has_value(), loaded.error());
	CDL_EXPECT(loaded->Extent == glm::uvec2(2, 1));
	CDL_EXPECT_EQ(loaded->Format, TextureFormat::Rgba8);
	CDL_EXPECT(ToTexels(loaded->Data) == (std::vector<Texel>{ { 10, 20, 30, 255 }, { 40, 50, 60, 255 } }));
}

// No flip on load: the file's top row comes first, which Triangle.slang's 1 - v relies on.
CDL_TEST_CASE(TextureLoader, KeepsTheFilesRowOrder, Unit)
{
	TempDirectory dir;

	const auto loaded = TextureLoader::LoadTexture(WritePPM(dir, "Column.ppm", { 1, 2 }, std::array{ kRed, kBlue }));

	CDL_CHECK_MSG(loaded.has_value(), loaded.error());
	CDL_EXPECT(loaded->Extent == glm::uvec2(1, 2));
	CDL_EXPECT(ToTexels(loaded->Data) == (std::vector<Texel>{ kRed, kBlue }));
}

// The loader only reports the error; TextureHandler is the one that logs it
CDL_TEST_CASE(TextureLoader, AMissingFileIsAnErrorNamingIt, Unit)
{
	TempDirectory dir;

	const auto loaded = TextureLoader::LoadTexture(dir / "Missing.png");

	CDL_CHECK_FALSE(loaded.has_value());
	CDL_EXPECT_MSG(loaded.error().contains("Missing.png"), loaded.error());
	CDL_EXPECT_MSG(loaded.error().contains("NotFound"), loaded.error());
}

// An empty file reads successfully, so it reaches the decoder with no bytes; both it and garbage must fail there.
CDL_TEST_CASE(TextureLoader, AFileItCannotDecodeIsAnErrorNamingIt, Unit)
{
	TempDirectory dir;
	const std::string garbage = "not an image";

	CDL_CHECK(Platform::WriteFile(dir / "Garbage.png", std::as_bytes(std::span(garbage))).has_value());
	CDL_CHECK(Platform::WriteFile(dir / "Empty.png", {}).has_value());

	for (const char* name : { "Garbage.png", "Empty.png" })
	{
		const auto loaded = TextureLoader::LoadTexture(dir / name);
		CDL_CHECK_MSG(!loaded.has_value(), name);
		CDL_EXPECT_MSG(loaded.error().contains(name), loaded.error());
	}
}
