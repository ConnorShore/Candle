// FrameGraphCapture, the render graph viewer's side of the packet: it sends one frame's request and keeps the snapshots that come
// back, and owns the preview target, sized from those snapshots. Packets are made by hand here in place of the render thread.
// The target comes from a real TexturePool, so these are Gpu tests.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "../RenderTestHelpers.h"

#include <Candle/Render/FrameGraph/FrameGraphCapture.h>
#include <Candle/Render/FramePacket.h>
#include <Candle/Render/Resources/TexturePool.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Gpu;
using Candle::Test::Type::Unit;

namespace {

	constexpr vk::Format kPreviewFormat = vk::Format::eR8G8B8A8Unorm;
	const glm::uvec2 kDepthExtent{ 8, 4 };

	struct TestCapture : TestTextures
	{
		FrameGraphCapture Capture{ Pool, kPreviewFormat };
		uint64_t NextFrame{ 1 };

		// The main thread acquiring a packet: what the render thread left in it comes back, and this frame's request goes out
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
	};

	FrameGraphSnapshot::Texture Transient(std::string name, glm::uvec2 extent)
	{
		return { .Name = std::move(name), .Imported = false, .Extent = extent, .Format = vk::Format::eD32Sfloat, .LatestVersion = 1 };
	}

	// A frame with a depth transient and the imported output, where the preview pass drew previewed if there is one
	FrameGraphSnapshot Snapshot(uint64_t frame, glm::uvec2 depthExtent = kDepthExtent, std::optional<uint32_t> previewed = std::nullopt)
	{
		FrameGraphSnapshot snapshot;
		snapshot.FrameIndex = frame;
		snapshot.Textures = {
			Transient("Depth", depthExtent),
			{ .Name = "Output", .Imported = true, .Extent = depthExtent, .Format = kPreviewFormat } };

		if (previewed)
		{
			snapshot.Passes.push_back({ .Name = std::string(kFrameGraphPreviewPassName), .Live = true, .AccessStart = 0, .AccessCount = 1 });
			snapshot.Accesses.push_back({ .ResourceIndex = *previewed, .Version = 1, .Usage = FGUsage::SampledFragment });
		}
		return snapshot;
	}

	FrameGraphCaptureRequest PreviewOf(std::string texture, bool capture = true)
	{
		return { .Snapshot = true, .PreviewTexture = std::move(texture), .CapturePreview = capture };
	}

}

// A viewer that closes stops asking, so a request mustn't stick around past the packet it was meant for
CDL_TEST_CASE(FrameGraphCapture, RequestRidesTheNextPacketOnly, Gpu)
{
	TestCapture test;

	test.Capture.Request({ .Snapshot = true });
	CDL_EXPECT(test.Exchange().Snapshot);
	CDL_EXPECT_FALSE(test.Exchange().Snapshot);
}

// A minimized frame hands back a packet it drew nothing for, which mustn't replace the last real snapshot
CDL_TEST_CASE(FrameGraphCapture, KeepsOnlySnapshotsTheRenderThreadCaptured, Gpu)
{
	TestCapture test;
	CDL_EXPECT(test.Capture.GetSnapshot() == nullptr);

	test.Exchange(Snapshot(7));
	CDL_CHECK(test.Capture.GetSnapshot() != nullptr);
	CDL_EXPECT_EQ(test.Capture.GetSnapshot()->FrameIndex, uint64_t{ 7 });

	test.Exchange();
	CDL_EXPECT_EQ(test.Capture.GetSnapshot()->FrameIndex, uint64_t{ 7 });
}

// Nothing has come back to size a target from yet, so the first request can't preview anything
CDL_TEST_CASE(FrameGraphCapture, NoPreviewTargetBeforeTheFirstSnapshot, Gpu)
{
	TestCapture test;

	test.Capture.Request(PreviewOf("Depth"));
	CDL_EXPECT_FALSE(test.Exchange().PreviewTarget.IsValid());
}

CDL_TEST_CASE(FrameGraphCapture, PreviewTargetIsSizedFromTheSnapshot, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestCapture test;
		test.Exchange(Snapshot(1));

		test.Capture.Request(PreviewOf("Depth"));
		const FrameGraphCaptureRequest sent = test.Exchange();
		CDL_CHECK(sent.PreviewTarget.IsValid());
		CDL_EXPECT_EQ(sent.PreviewTexture, std::string("Depth"));

		const TextureInfo info = test.Pool.GetTextureInfo(sent.PreviewTarget);
		CDL_EXPECT(info.Desc.Extent == kDepthExtent);
		CDL_EXPECT_EQ(info.Desc.Format, kPreviewFormat);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The snapshot is how the capture learns the preview landed, so a preview always brings one back
CDL_TEST_CASE(FrameGraphCapture, PreviewAlwaysAsksForASnapshot, Gpu)
{
	TestCapture test;
	test.Exchange(Snapshot(1));

	test.Capture.Request({ .PreviewTexture = "Depth", .CapturePreview = true });
	CDL_EXPECT(test.Exchange().Snapshot);
}

// Resizing the window resizes depth, and the old target may still be in a frame in flight, so it's released, not reused
CDL_TEST_CASE(FrameGraphCapture, PreviewTargetIsReplacedWhenTheTextureResizes, Gpu)
{
	TestCapture test;
	test.Exchange(Snapshot(1));
	test.Capture.Request(PreviewOf("Depth"));
	const GPUTextureHandle first = test.Exchange().PreviewTarget;

	const glm::uvec2 resized{ 16, 8 };
	test.Capture.Request(PreviewOf("Depth"));
	const GPUTextureHandle second = test.Exchange(Snapshot(2, resized)).PreviewTarget;

	CDL_CHECK(second.IsValid());
	CDL_EXPECT(second != first);
	CDL_EXPECT_FALSE(test.Pool.IsAlive(first));
	CDL_EXPECT(test.Pool.GetTextureInfo(second).Desc.Extent == resized);
}

CDL_TEST_CASE(FrameGraphCapture, PreviewTargetIsReusedWhileTheSizeHolds, Gpu)
{
	TestCapture test;
	test.Exchange(Snapshot(1));
	test.Capture.Request(PreviewOf("Depth"));
	const GPUTextureHandle first = test.Exchange().PreviewTarget;

	test.Capture.Request(PreviewOf("Depth"));
	CDL_EXPECT(test.Exchange(Snapshot(2)).PreviewTarget == first);
}

// Paused, the target keeps the frame it last drew, so it's kept but not handed to the render thread
CDL_TEST_CASE(FrameGraphCapture, PausedPreviewKeepsItsTargetUndrawn, Gpu)
{
	TestCapture test;
	test.Exchange(Snapshot(1));
	test.Capture.Request(PreviewOf("Depth"));
	const GPUTextureHandle target = test.Exchange().PreviewTarget;

	test.Capture.Request(PreviewOf("Depth", false));
	CDL_EXPECT_FALSE(test.Exchange().PreviewTarget.IsValid());
	CDL_EXPECT(test.Pool.IsAlive(target));

	test.Capture.Request(PreviewOf("Depth"));
	CDL_EXPECT(test.Exchange().PreviewTarget == target);
}

// The viewer closing stops its requests, which is what frees the target
CDL_TEST_CASE(FrameGraphCapture, PreviewTargetIsReleasedOnceNothingAsksForIt, Gpu)
{
	TestCapture test;
	test.Exchange(Snapshot(1));
	test.Capture.Request(PreviewOf("Depth"));
	const GPUTextureHandle target = test.Exchange().PreviewTarget;

	test.Exchange();
	CDL_EXPECT_FALSE(test.Pool.IsAlive(target));
	CDL_EXPECT_FALSE(test.Capture.GetPreview().has_value());
}

// Imports are in the texture inspector already, and a name that isn't in the graph has nothing to show
CDL_TEST_CASE(FrameGraphCapture, OnlyTransientsInTheSnapshotGetATarget, Gpu)
{
	TestCapture test;
	test.Exchange(Snapshot(1));

	test.Capture.Request(PreviewOf("Output"));
	CDL_EXPECT_FALSE(test.Exchange().PreviewTarget.IsValid());

	test.Capture.Request(PreviewOf("Missing"));
	CDL_EXPECT_FALSE(test.Exchange().PreviewTarget.IsValid());
}

// A new target's contents are undefined until a frame that carried it draws into it, and only its snapshot can say so
CDL_TEST_CASE(FrameGraphCapture, PreviewIsShownOnlyOnceASnapshotShowsItDrawn, Gpu)
{
	TestCapture test;
	test.Exchange(Snapshot(1));

	test.Capture.Request(PreviewOf("Depth"));
	const uint64_t sentIn = test.NextFrame;
	const GPUTextureHandle target = test.Exchange().PreviewTarget;
	CDL_EXPECT_FALSE(test.Capture.GetPreview().has_value());

	// Drawn into the target the frame before, which was another target
	test.Capture.Request(PreviewOf("Depth"));
	test.Exchange(Snapshot(sentIn - 1, kDepthExtent, 0));
	CDL_EXPECT_FALSE(test.Capture.GetPreview().has_value());

	test.Capture.Request(PreviewOf("Depth"));
	test.Exchange(Snapshot(sentIn, kDepthExtent, 0));
	const std::optional<FrameGraphPreview>& preview = test.Capture.GetPreview();
	CDL_CHECK(preview.has_value());
	CDL_EXPECT(preview->Target == target);
	CDL_EXPECT_EQ(preview->Texture, std::string("Depth"));
	CDL_EXPECT_EQ(preview->FrameIndex, sentIn);
}

CDL_TEST_CASE(FrameGraphCapture, OnlyFloatFormatsWithoutStencilArePreviewable, Unit)
{
	CDL_EXPECT(IsPreviewableFormat(vk::Format::eR8G8B8A8Unorm));
	CDL_EXPECT(IsPreviewableFormat(vk::Format::eR16G16B16A16Sfloat));
	CDL_EXPECT(IsPreviewableFormat(vk::Format::eD32Sfloat));
	CDL_EXPECT(IsPreviewableFormat(vk::Format::eD16Unorm));
	CDL_EXPECT_FALSE(IsPreviewableFormat(vk::Format::eR32Uint));
	CDL_EXPECT_FALSE(IsPreviewableFormat(vk::Format::eR16G16Sint));
	CDL_EXPECT_FALSE(IsPreviewableFormat(vk::Format::eD24UnormS8Uint));
}
