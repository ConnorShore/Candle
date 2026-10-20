// RenderManager's frame loop end to end on a real window: frame packets drawn by the engine's triangle pipeline, the
// timeline-paced frame slot, the swapchain handoff, surface reports, and the idle wait in its destructor. Each test
// fails on any validation message, teardown's included.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "RenderTestHelpers.h"

#include <Candle/Asset/AssetManager.h>
#include <Candle/Core/Job/JobSystem.h>
#include <Candle/Render/Assets/MeshAsset.h>
#include <Candle/Render/FramePacket.h>
#include <Candle/Render/RenderManager.h>

#include <cstdint>
#include <filesystem>
#include <format>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Gpu;

namespace {

	// Renderer loads the engine's compiled triangle, a build output rather than a checked-in file.
	const std::filesystem::path kTriangleShaderPath = "Candle/res/shaders/bin/Triangle.spv";

	// Construction order is dependency order, so the RenderManager is destroyed first and waits for the GPU.
	struct TestRenderManager
	{
		WindowingBackend Backend;
		Window AppWindow{ TestWindowSpec() };
		JobSystem Jobs;	// Never started; RenderManager only reads its worker count
		RenderManager Manager{ StrictRenderSpec(), ApplicationInfo{ .Name = "Candle-Test" }, AppWindow, Jobs };

		GPUMeshHandle CreateTriangle() { return Manager.GetGeometryPool().CreateMesh(kTriangleVertices, kTriangleIndices); }
	};

	// One packet per frame, as the simulation sends them. The renderer draws the packet's first mesh, and time advances
	// so each slot's FrameData is rewritten while earlier frames may still be in flight.
	void RenderFrames(RenderManager& renderManager, GPUMeshHandle mesh, int count)
	{
		for (int frame = 0; frame < count; ++frame)
		{
			renderManager.SubmitFrame(FramePacket{ .Meshes = { mesh }, .Time = frame / 60.0 });
			renderManager.RenderThreadMain();
		}
	}

	// An empty submit's timeline value; the gap between two of them counts the frames submitted in between.
	uint64_t TimelineProbe(RenderManager& renderManager)
	{
		return renderManager.GetRenderDevice().Submit(QueueType::Graphics, {});
	}

}

// More frames than frame slots or swapchain images, so every slot is waited on and every image comes back.
CDL_TEST_CASE(RenderManager, RendersFramesAndShutsDownCleanly, Gpu)
{
	SkipUnlessCompiled(kTriangleShaderPath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestRenderManager test;
		const GPUMeshHandle mesh = test.CreateTriangle();

		const uint64_t before = TimelineProbe(test.Manager);
		RenderFrames(test.Manager, mesh, 10);
		CDL_EXPECT_EQ(TimelineProbe(test.Manager), before + 11);	// One submit per frame
	}
	ExpectNoVulkanDebugMessages(logs);
}

// A minimized surface has no extent to build a swapchain at, so frames are skipped until it is restored.
CDL_TEST_CASE(RenderManager, SkipsFramesWhileMinimized, Gpu)
{
	SkipUnlessCompiled(kTriangleShaderPath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestRenderManager test;
		const GPUMeshHandle mesh = test.CreateTriangle();

		test.Manager.OnSurfaceMinimized(true);
		const uint64_t minimized = TimelineProbe(test.Manager);
		RenderFrames(test.Manager, mesh, 3);
		CDL_EXPECT_EQ(TimelineProbe(test.Manager), minimized + 1);

		test.Manager.OnSurfaceMinimized(false);
		const uint64_t restored = TimelineProbe(test.Manager);
		RenderFrames(test.Manager, mesh, 3);
		CDL_EXPECT_EQ(TimelineProbe(test.Manager), restored + 4);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// A swapchain can't be created at a zero extent, which some platforms report instead of minimizing.
CDL_TEST_CASE(RenderManager, SkipsFramesWhileTheSurfaceHasNoArea, Gpu)
{
	SkipUnlessCompiled(kTriangleShaderPath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestRenderManager test;
		const GPUMeshHandle mesh = test.CreateTriangle();

		test.Manager.OnSurfaceResized({ 0, 0 });
		const uint64_t empty = TimelineProbe(test.Manager);
		RenderFrames(test.Manager, mesh, 3);
		CDL_EXPECT_EQ(TimelineProbe(test.Manager), empty + 1);

		test.Manager.OnSurfaceResized(test.AppWindow.GetSize());
		const uint64_t restored = TimelineProbe(test.Manager);
		RenderFrames(test.Manager, mesh, 3);
		CDL_EXPECT_EQ(TimelineProbe(test.Manager), restored + 4);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// A reported resize rebuilds the swapchain before the next acquire, and frames carry on at the new size.
CDL_TEST_CASE(RenderManager, RebuildsTheSwapChainAfterAResize, Gpu)
{
	SkipUnlessCompiled(kTriangleShaderPath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestRenderManager test;
		const GPUMeshHandle mesh = test.CreateTriangle();
		RenderFrames(test.Manager, mesh, 2);

		const glm::uvec2 before = test.AppWindow.GetSize();
		test.AppWindow.Resize(before.x + 160, before.y + 120);
		const glm::uvec2 after = test.AppWindow.GetSize();
		CDL_CHECK_MSG(after != before, std::format("the window stayed {}x{}, so nothing would be rebuilt", before.x, before.y));
		test.Manager.OnSurfaceResized(after);

		const uint64_t resized = TimelineProbe(test.Manager);
		RenderFrames(test.Manager, mesh, 4);
		CDL_EXPECT_EQ(TimelineProbe(test.Manager), resized + 5);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The application's path for meshes: RenderManager hands its mesh handler to the AssetManager, which uploads on load.
CDL_TEST_CASE(RenderManager, RegisteredHandlersUploadMeshAssets, Gpu)
{
	SkipUnlessCompiled(kTriangleShaderPath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestRenderManager test;
		AssetManager assets;	// Destroyed before the RenderManager whose GeometryPool its handler uploads into
		test.Manager.RegisterAssetHandlers(assets);

		SharedPtr<MeshAsset> triangle = SharedPtr<MeshAsset>::Create(MeshData{
			.Vertices = { kTriangleVertices.begin(), kTriangleVertices.end() },
			.Indices = { kTriangleIndices.begin(), kTriangleIndices.end() } });
		assets.LoadAsset("Triangle", triangle);

		CDL_CHECK(triangle->GetGPUHandle().IsValid());
		RenderFrames(test.Manager, triangle->GetGPUHandle(), 3);
	}
	ExpectNoVulkanDebugMessages(logs);
}
