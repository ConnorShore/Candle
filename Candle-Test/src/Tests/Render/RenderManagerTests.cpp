// RenderManager's frame loop end to end on a real window: frame packets drawn by the engine's triangle pipeline, the
// timeline-paced frame slot, the swapchain handoff, and the idle wait in its destructor. Fails on any validation
// message, teardown's included.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "RenderTestHelpers.h"

#include <Candle/Core/Job/JobSystem.h>
#include <Candle/Render/FramePacket.h>
#include <Candle/Render/RenderManager.h>
#include <Candle/Render/ShaderInterop/Vertex.h>

#include <cstdint>
#include <filesystem>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Gpu;

namespace {

	// Renderer loads the engine's compiled triangle, a build output rather than a checked-in file.
	const std::filesystem::path kTriangleShaderPath = "Candle/res/shaders/bin/Triangle.spv";

}

// More frames than frame slots or swapchain images, so every slot is waited on and every image comes back.
CDL_TEST_CASE(RenderManager, RendersFramesAndShutsDownCleanly, Gpu)
{
	constexpr int kFrames = 10;
	SkipUnlessCompiled(kTriangleShaderPath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		WindowingBackend backend;
		Window window(TestWindowSpec());
		JobSystem jobSystem;	// Never started; RenderManager only reads its worker count
		RenderManager renderManager(StrictRenderSpec(), ApplicationInfo{ .Name = "Candle-Test" }, window, jobSystem);

		// The renderer draws the packet's first mesh, so every frame needs a packet holding one
		const GPU::Vertex vertices[] = { { { 0.5f, -0.5f, 0.0f } }, { { 0.0f, 0.5f, 0.0f } }, { { -0.5f, -0.5f, 0.0f } } };
		const uint32_t indices[] = { 0, 1, 2 };
		const GPUMeshHandle mesh = renderManager.GetGeometryPool().CreateMesh(vertices, indices);

		// Time advances so each frame slot's FrameData is rewritten while earlier frames may still be in flight
		for (int frame = 0; frame < kFrames; ++frame)
		{
			renderManager.SubmitFrame(FramePacket{ .Meshes = { mesh }, .Time = frame / 60.0 });
			renderManager.RenderThreadMain();
		}
	}
	ExpectNoVulkanDebugMessages(logs);
}
