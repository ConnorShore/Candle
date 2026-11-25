// RenderGraphPanel, fed snapshots through a real FrameGraphCapture as RenderManager would, and read through what it prints. The
// matrix's pass names are drawn rotated, straight into the draw list, so they never reach ImGui's log; texture names and the
// summary do. The capture needs a real TexturePool for its preview target, so these are Gpu tests.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "PanelTestHelpers.h"
#include "../../Render/RenderTestHelpers.h"

#include <Candle/Debug/Panels/RenderGraphPanel.h>
#include <Candle/Render/FrameGraph/FrameGraphCapture.h>
#include <Candle/Render/FramePacket.h>

#include <imgui.h>

#include <format>
#include <optional>
#include <string>
#include <utility>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Gpu;

namespace {

	constexpr vk::Format kColorFormat = vk::Format::eR8G8B8A8Unorm;
	const glm::uvec2 kExtent{ 8, 4 };

	struct TestPanel
	{
		TestTextures Textures;
		FrameGraphCapture Capture{ Textures.Pool, kColorFormat };
		HeadlessGui Gui;
		RenderGraphPanel Panel{ Capture };
		uint64_t NextFrame{ 1 };

		// Pinned to the corner, so the toolbar is somewhere a click can find
		void Draw()
		{
			ImGui::SetNextWindowPos({ 0.0f, 0.0f });
			Panel.OnRender({});
		}

		std::string Drawn() { return Gui.Capture([this] { Draw(); }); }

		// The main thread acquiring a packet: the snapshot comes back, and whatever the panel asked for goes out
		FrameGraphCaptureRequest Exchange(std::optional<FrameGraphSnapshot> returned = std::nullopt)
		{
			FramePacket packet;
			packet.FrameIndex = NextFrame++;
			if (returned)
			{
				packet.FrameGraphSnapshot = std::move(*returned);
				packet.FrameGraphSnapshot.Captured = true;
			}
			Capture.ExchangeWithPacket(packet);
			return packet.FrameGraphCapture;
		}

		// Pause is the window's first item, under the title bar
		void ClickPause()
		{
			const ImGuiStyle& style = ImGui::GetStyle();
			const float frameHeight = ImGui::GetFontSize() + style.FramePadding.y * 2.0f;
			Gui.Click({ style.WindowPadding.x + frameHeight * 0.5f, frameHeight + style.WindowPadding.y + frameHeight * 0.5f }, [this] { Draw(); });
		}
	};

	// A deferred frame with a debug view nobody turned on: GBuffer feeds Lighting, which writes the output, and DebugView is culled
	FrameGraphSnapshot DeferredFrame(uint64_t frame)
	{
		constexpr vk::ImageUsageFlags kTargetUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;

		FrameGraphSnapshot snapshot;
		snapshot.FrameIndex = frame;
		snapshot.OutputExtent = kExtent;
		snapshot.Textures = {
			{ .Name = "Albedo", .Extent = kExtent, .Format = kColorFormat, .Usage = kTargetUsage, .LatestVersion = 1, .CacheIndex = 0, .BindlessIndex = 0 },
			{ .Name = "DebugOverlay", .Extent = kExtent, .Format = kColorFormat, .LatestVersion = 1 },
			{ .Name = "Output", .Imported = true, .Extent = kExtent, .Format = kColorFormat, .Usage = kTargetUsage, .LatestVersion = 1 } };
		snapshot.Passes = {
			{ .Name = "GBuffer", .Live = true, .AccessStart = 0, .AccessCount = 1 },
			{ .Name = "DebugView", .Live = false, .AccessStart = 1, .AccessCount = 1 },
			{ .Name = "Lighting", .Live = true, .AccessStart = 2, .AccessCount = 2 } };
		snapshot.Accesses = {
			{ .ResourceIndex = 0, .Version = 0, .Usage = FGUsage::ColorAttachment, .Write = true, .LoadOp = FGLoad::Clear },
			{ .ResourceIndex = 1, .Version = 0, .Usage = FGUsage::ColorAttachment, .Write = true, .LoadOp = FGLoad::Clear },
			{ .ResourceIndex = 0, .Version = 1, .Usage = FGUsage::SampledFragment, .SrcPassIndex = 0 },
			{ .ResourceIndex = 2, .Version = 0, .Usage = FGUsage::ColorAttachment, .Write = true, .LoadOp = FGLoad::Clear } };
		return snapshot;
	}

}

CDL_TEST_CASE(RenderGraphPanel, WaitsForTheFirstSnapshot, Gpu)
{
	TestPanel test;

	const std::string text = test.Drawn();
	CDL_EXPECT_MSG(text.contains("Waiting for the render thread's first snapshot"), text);
}

CDL_TEST_CASE(RenderGraphPanel, SummarisesAndListsTheSnapshot, Gpu)
{
	TestPanel test;
	test.Exchange(DeferredFrame(5));

	const std::string text = test.Drawn();
	CDL_EXPECT_MSG(text.contains("Frame 5"), text);
	CDL_EXPECT_MSG(text.contains("Passes: 3 (2 live, 1 culled)"), text);
	CDL_EXPECT_MSG(text.contains("Textures: 3 (1 imported, 2 transient)"), text);
	for (const char* texture : { "Albedo", "DebugOverlay", "Output" })
		CDL_EXPECT_MSG(text.contains(texture), std::format("{} isn't listed: {}", texture, text));
}

// Only while it renders, so a closed or hidden panel stops the render thread copying the graph
CDL_TEST_CASE(RenderGraphPanel, AsksForASnapshotOnlyWhileItRenders, Gpu)
{
	TestPanel test;

	test.Drawn();
	CDL_EXPECT(test.Exchange().Snapshot);
	CDL_EXPECT_FALSE(test.Exchange().Snapshot);
}

// The newer snapshot keeps arriving while paused, but the graph on screen holds still until it's unpaused
CDL_TEST_CASE(RenderGraphPanel, PauseHoldsTheFrameItStoppedOn, Gpu)
{
	TestPanel test;
	test.Exchange(DeferredFrame(5));
	CDL_CHECK(test.Drawn().contains("Frame 5"));

	test.ClickPause();
	test.Exchange(DeferredFrame(6));

	std::string text = test.Drawn();
	CDL_EXPECT_MSG(text.contains("PAUSED"), text);
	CDL_EXPECT_MSG(text.contains("Frame 5"), text);
	CDL_EXPECT(test.Exchange().Snapshot);	// Still asking, so unpausing picks up a current frame

	test.ClickPause();
	text = test.Drawn();
	CDL_EXPECT_MSG(text.contains("Frame 6"), text);
	CDL_EXPECT_FALSE(text.contains("PAUSED"));
}
