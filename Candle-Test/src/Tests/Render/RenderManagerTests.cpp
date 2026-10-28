// RenderManager's frame loop end to end on a real window and its real render thread: frame packets drawn by the engine's
// triangle pipeline, the pump that paces the main thread, skipped frames, and deferred releases riding the packets.
// Each test fails on any validation message, teardown's included.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "RenderTestHelpers.h"

#include <Candle/Asset/AssetManager.h>
#include <Candle/Core/Job/JobSystem.h>
#include <Candle/Render/Assets/MeshAsset.h>
#include <Candle/Render/Assets/Texture.h>
#include <Candle/Render/FramePacket.h>
#include <Candle/Render/FramePump.h>
#include <Candle/Render/RenderConstants.h>
#include <Candle/Render/RenderManager.h>
#include <Candle/Render/SamplerType.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <format>
#include <utility>
#include <vector>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Gpu;

namespace {

	// Renderer loads the engine's compiled triangle, a build output rather than a checked-in file.
	const std::filesystem::path kTriangleShaderPath = "Candle/res/shaders/bin/Triangle.spv";

	// ~RenderManager doesn't join, so a test that failed between spawn and join would terminate the run on ~std::thread.
	class RenderThreadScope
	{
	public:
		explicit RenderThreadScope(RenderManager& manager) : m_Manager(manager) { m_Manager.SpawnRenderThread(); }
		~RenderThreadScope() { Join(); }

		void Join() { if (std::exchange(m_Running, false)) m_Manager.JoinRenderThread(); }

	private:
		RenderManager& m_Manager;
		bool m_Running{ true };
	};

	// Construction order is dependency order: the render thread is joined first, then the assets go, then the RenderManager
	// waits for the GPU.
	struct TestRenderManager
	{
		WindowingBackend Backend;
		Window AppWindow{ TestWindowSpec() };
		JobSystem Jobs;	// Never started; RenderManager only reads its worker count
		TempDirectory Files;
		RenderManager Manager{ StrictRenderSpec(), ApplicationInfo{ .Name = "Candle-Test" }, AppWindow, Jobs };
		AssetManager Assets;	// After the RenderManager whose pools its handlers upload into
		GPUTextureHandle DefaultTexture = RegisterHandlersAndLoadTexture();	// Before the render thread, as Application loads its assets
		RenderThreadScope RenderThread{ Manager };

		GPUMeshHandle CreateTriangle() { return Manager.GetGeometryPool().CreateMesh(kTriangleVertices, kTriangleIndices); }

		// The application's path for textures: a file, decoded and uploaded by the handler RenderManager registers.
		GPUTextureHandle LoadTexture(const std::string& name, const Texel& colour)
		{
			[[maybe_unused]] const uint64_t uuid = Assets.LoadAsset<Texture>(name, WritePPM(Files, name + ".ppm", { 1, 1 }, std::array<Texel, 1>{ colour }));
			return Assets.GetAsset<Texture>(name)->GetGPUHandle();
		}

	private:
		// Every packet's draw samples a material, so each test needs a texture before its first frame
		GPUTextureHandle RegisterHandlersAndLoadTexture()
		{
			Manager.RegisterAssetHandlers(Assets);
			return LoadTexture("Texture", kWhite);
		}
	};

	// What a packet says about the surface, which Application::Run reads off the window each frame.
	struct Surface
	{
		glm::uvec2 Size;
		bool Minimized{ false };
	};

	Surface WindowSurface(const Window& window) { return { .Size = window.GetSize(), .Minimized = window.IsMinimized() }; }

	// An empty submit's timeline value; every packet the render thread takes costs exactly one submit, drawn or skipped.
	uint64_t TimelineProbe(RenderManager& manager)
	{
		return manager.GetRenderDevice().Submit(QueueType::Graphics, {});
	}

	// Free runs the frame loop as Application does. DrainGPU waits for all GPU work before each submit, so the render
	// thread finds every packet two behind complete: a slot released before packet k is freed by packet k + 2 at the
	// latest, and is back on the main thread once packet k + 4 is begun.
	enum class Pacing { Free, DrainGPU };
	constexpr int kReleaseRoundTrip = 2 * kFramePacketCount + 1;

	// One packet per frame. The renderer draws the packet's first mesh with its first material, and time advances so each
	// slot's FrameData is rewritten while earlier frames may still be in flight.
	void SubmitFrames(TestRenderManager& test, int count, GPUMeshHandle mesh, const Surface& surface, Pacing pacing = Pacing::Free,
		GPUTextureHandle texture = {})
	{
		RenderManager& manager = test.Manager;
		for (int frame = 0; frame < count; ++frame)
		{
			FramePacket& packet = manager.BeginFramePacket();
			packet.Meshes = { mesh };
			packet.Materials = { texture.IsValid() ? texture : test.DefaultTexture };
			packet.SamplerIndices = { SamplerType::LinearWrap };
			packet.Time = frame / 60.0;
			packet.Minimized = surface.Minimized;
			packet.SurfaceSize = surface.Size;

			if (pacing == Pacing::DrainGPU)
				manager.GetRenderDevice().WaitFor(QueueType::Graphics, TimelineProbe(manager));
			manager.SubmitFramePacket(packet);
		}
	}

	// Minimize and Restore can land a few pumps later; only the state they leave matters here, not the events.
	bool WaitForMinimized(const Window& window, bool minimized)
	{
		std::vector<PlatformEvent> events;
		const Timer timer;
		while (window.IsMinimized() != minimized && timer.ElapsedMilliseconds() < 2000.0)
		{
			Platform::PumpEvents(events);
			Platform::SleepCurrentThread(5);
		}
		return window.IsMinimized() == minimized;
	}

}

// More frames than frame slots, swapchain images or packets, so every slot is waited on and every image and packet comes
// back. The main thread runs at most kFramePacketCount packets ahead, which bounds the timeline from below.
CDL_TEST_CASE(RenderManager, RendersFramesAndShutsDownCleanly, Gpu)
{
	SkipUnlessCompiled(kTriangleShaderPath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestRenderManager test;
		const GPUMeshHandle mesh = test.CreateTriangle();
		constexpr int kFrames = 10;

		const uint64_t before = TimelineProbe(test.Manager);
		SubmitFrames(test, kFrames, mesh, WindowSurface(test.AppWindow));
		const uint64_t after = TimelineProbe(test.Manager);

		CDL_EXPECT_GE(after, before + kFrames - kFramePacketCount + 1);
		CDL_EXPECT_LE(after, before + kFrames + 1);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// A quit can land before the render thread has run at all, so a join straight after the spawn must still return.
// Fails by hanging rather than by assertion.
CDL_TEST_CASE(RenderManager, JoinRightAfterSpawnReturns, Gpu)
{
	SkipUnlessCompiled(kTriangleShaderPath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestRenderManager test;
		test.RenderThread.Join();
	}
	ExpectNoVulkanDebugMessages(logs);
}

// A minimized surface has a zero extent, so building a swapchain for it fails validation; its frames are skipped, but
// each still hands its packet back, or the main thread hangs once the pump is full.
CDL_TEST_CASE(RenderManager, SkipsFramesWhileMinimized, Gpu)
{
	SkipUnlessCompiled(kTriangleShaderPath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestRenderManager test;
		const GPUMeshHandle mesh = test.CreateTriangle();
		const Surface visible = WindowSurface(test.AppWindow);
		SubmitFrames(test, 3, mesh, visible);

		// Skipped first, so the last drawn frame has presented before the window goes; one drawn after could rebuild at zero
		SubmitFrames(test, kFramePacketCount + 1, mesh, { .Size = visible.Size, .Minimized = true });

		test.AppWindow.Minimize();
		CDL_CHECK_MSG(WaitForMinimized(test.AppWindow, true), "the window never reported minimized");
		SubmitFrames(test, 10, mesh, WindowSurface(test.AppWindow));

		test.AppWindow.Restore();
		CDL_CHECK_MSG(WaitForMinimized(test.AppWindow, false), "the window never reported restored");
		SubmitFrames(test, 3, mesh, WindowSurface(test.AppWindow));
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Some platforms report a zero extent instead of minimizing. Windows dictates the swapchain's extent, so here a frame
// drawn at zero would pass too; what fails is a skipped frame keeping its packet, which hangs the main thread.
CDL_TEST_CASE(RenderManager, KeepsTakingPacketsWhileTheSurfaceHasNoArea, Gpu)
{
	SkipUnlessCompiled(kTriangleShaderPath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestRenderManager test;
		const GPUMeshHandle mesh = test.CreateTriangle();

		SubmitFrames(test, 10, mesh, { .Size = { 0, 0 } });
		SubmitFrames(test, 3, mesh, WindowSurface(test.AppWindow));
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Packets queued at the old size are drawn after the resize lands. The swapchain takes the surface's extent whatever a
// packet says, so this can't tell a rebuild from none; it checks the frame loop survives one cleanly.
CDL_TEST_CASE(RenderManager, RendersAcrossAResize, Gpu)
{
	SkipUnlessCompiled(kTriangleShaderPath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestRenderManager test;
		const GPUMeshHandle mesh = test.CreateTriangle();
		SubmitFrames(test, 3, mesh, WindowSurface(test.AppWindow));

		const glm::uvec2 before = test.AppWindow.GetSize();
		test.AppWindow.Resize(before.x + 160, before.y + 120);
		const glm::uvec2 after = test.AppWindow.GetSize();
		CDL_CHECK_MSG(after != before, std::format("the window stayed {}x{}, so nothing would be rebuilt", before.x, before.y));

		SubmitFrames(test, 4, mesh, WindowSurface(test.AppWindow));
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The mesh is released while frames drawing it may be in flight; its slot only comes back once the render thread has
// retired it and handed it back through a packet. A free that came too early would pass here, since a triangle's frames
// finish long before the render thread frees anything; GPUResourcePoolTests fake the timeline to catch that.
CDL_TEST_CASE(RenderManager, ReleasedMeshSlotComesBackOnceItsFramesRetire, Gpu)
{
	SkipUnlessCompiled(kTriangleShaderPath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestRenderManager test;
		const GPUMeshHandle kept = test.CreateTriangle();
		const GPUMeshHandle released = test.CreateTriangle();
		const Surface surface = WindowSurface(test.AppWindow);

		SubmitFrames(test, kMaxFramesInFlight, released, surface);
		test.Manager.GetGeometryPool().ReleaseMesh(released);
		CDL_EXPECT_FALSE(test.Manager.GetGeometryPool().IsAlive(released));

		SubmitFrames(test, kReleaseRoundTrip, kept, surface, Pacing::DrainGPU);
		CDL_EXPECT_EQ(test.CreateTriangle().Index, released.Index);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// A skipped frame draws nothing, but the releases its packet carries must still retire, or a minimized app leaks.
CDL_TEST_CASE(RenderManager, ReleasesStillRetireWhileFramesAreSkipped, Gpu)
{
	SkipUnlessCompiled(kTriangleShaderPath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestRenderManager test;
		const GPUMeshHandle kept = test.CreateTriangle();
		const GPUMeshHandle released = test.CreateTriangle();
		const Surface visible = WindowSurface(test.AppWindow);

		SubmitFrames(test, kMaxFramesInFlight, released, visible);
		test.Manager.GetGeometryPool().ReleaseMesh(released);

		SubmitFrames(test, kReleaseRoundTrip, kept, { .Size = visible.Size, .Minimized = true }, Pacing::DrainGPU);
		CDL_EXPECT_EQ(test.CreateTriangle().Index, released.Index);
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

		SharedPtr<MeshAsset> triangle = SharedPtr<MeshAsset>::Create("Triangle", MeshData{
			.Vertices = { kTriangleVertices.begin(), kTriangleVertices.end() },
			.Indices = { kTriangleIndices.begin(), kTriangleIndices.end() } });
		assets.LoadAsset(triangle);

		CDL_CHECK(triangle->GetGPUHandle().IsValid());
		SubmitFrames(test, 3, triangle->GetGPUHandle(), WindowSurface(test.AppWindow));
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The application's path for textures: RenderManager hands its texture handler to the AssetManager, which uploads the
// file on load and publishes it in the bindless table, so frames can draw with it at once.
CDL_TEST_CASE(RenderManager, RegisteredHandlersUploadTextureAssets, Gpu)
{
	SkipUnlessCompiled(kTriangleShaderPath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestRenderManager test;
		const GPUTextureHandle texture = test.LoadTexture("Green", kGreen);
		CDL_CHECK(texture.IsValid());
		CDL_EXPECT_NE(texture.Index, test.DefaultTexture.Index);

		SubmitFrames(test, 3, test.CreateTriangle(), WindowSurface(test.AppWindow), Pacing::Free, texture);
	}
	ExpectNoVulkanDebugMessages(logs);
}
