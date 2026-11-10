// TextureInspectorPanel over a real TexturePool, which it reads directly. Selecting a row needs its position on screen, which a
// test can't get from ImGui, so these stop at the list.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "PanelTestHelpers.h"
#include "../../Render/RenderTestHelpers.h"

#include <Candle/Debug/Panels/TextureInspectorPanel.h>

#include <array>
#include <span>
#include <string>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Gpu;

namespace {

	struct TestInspector
	{
		TestTextures Textures;
		HeadlessGui Gui;
		TextureInspectorPanel Panel{ Textures.Pool };

		GPUTextureHandle CreateNamedTexture(std::string name)
		{
			static constexpr std::array<Texel, 1> kTexel{ kWhite };
			return Textures.Pool.CreateTexture({
				.Data = std::as_bytes(std::span(kTexel)),
				.Extent = { 1, 1 },
				.Format = TextureFormat::Rgba8,
				.ColorSpace = TextureColorSpace::Linear,
				.DebugName = std::move(name) });
		}

		std::string Drawn() { return Gui.Capture([this] { Panel.OnRender({}); }); }
	};

}

CDL_TEST_CASE(TextureInspectorPanel, ListsEveryLiveTextureWithItsKind, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestInspector test;
		test.CreateNamedTexture("Brick");
		test.Textures.Pool.CreateRenderTarget({ .Extent = { 4, 4 }, .Format = vk::Format::eR8G8B8A8Unorm, .DebugName = "Scene" });
		test.Textures.Pool.CreateRenderTarget({ .Extent = { 4, 4 }, .Format = vk::Format::eR8G8B8A8Unorm,
			.Usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled, .DebugName = "Compute" });

		const std::string text = test.Drawn();
		CDL_EXPECT_MSG(text.contains("4 textures"), text);
		CDL_EXPECT_MSG(text.contains("Default Texture 0 | Default"), text);
		CDL_EXPECT_MSG(text.contains("Brick | Texture"), text);
		CDL_EXPECT_MSG(text.contains("Scene | Render target"), text);
		CDL_EXPECT_MSG(text.contains("Compute | Storage"), text);
	}
	ExpectNoVulkanDebugMessages(logs);
}

CDL_TEST_CASE(TextureInspectorPanel, ReleasedTextureLeavesTheListAndCountsAsPending, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestInspector test;
		const GPUTextureHandle brick = test.CreateNamedTexture("Brick");
		CDL_EXPECT(test.Drawn().contains("Brick"));

		test.Textures.Pool.ReleaseTexture(brick);

		const std::string text = test.Drawn();
		CDL_EXPECT_FALSE(text.contains("Brick"));
		CDL_EXPECT_MSG(text.contains("1 textures"), text);
		CDL_EXPECT_MSG(text.contains("Pending 1"), text);
	}
	ExpectNoVulkanDebugMessages(logs);
}

CDL_TEST_CASE(TextureInspectorPanel, StartsWithNothingSelected, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestInspector test;

		const std::string text = test.Drawn();
		CDL_EXPECT_MSG(text.contains("Select a texture to preview it"), text);
	}
	ExpectNoVulkanDebugMessages(logs);
}
