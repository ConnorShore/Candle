// GeometryPool on a real device: mesh creation, its draw info, slot reuse through the free list, and the buffers a
// released mesh gives back once its frame retires. Each test fails on any validation message.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "RenderTestHelpers.h"

#include <Candle/Render/Resources/GeometryPool.h>
#include <Candle/Render/Resources/GPUHandles.h>
#include <Candle/Render/ShaderInterop/Vertex.h>

#include <array>
#include <cstdint>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Gpu;

namespace {

	GPUMeshHandle CreateTriangle(GeometryPool& pool)
	{
		return pool.CreateMesh(kTriangleVertices, kTriangleIndices);
	}

}

CDL_TEST_CASE(GeometryPool, CreateMeshReturnsAValidHandleAndItsDrawInfo, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestGeometry gpu;
		const GPUMeshHandle mesh = CreateTriangle(gpu.Pool);
		const MeshDrawInfo info = gpu.Pool.GetMeshInfo(mesh);

		CDL_EXPECT(mesh.IsValid());
		CDL_EXPECT_EQ(info.IndexCount, static_cast<uint32_t>(kTriangleIndices.size()));
		CDL_EXPECT_NE(info.VerticesAddr, 0u);
		CDL_EXPECT(info.Indices);

		RunReleaseFrame(gpu);
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 2u);	// Vertices and indices, once the upload's staging is drained
	}
	ExpectNoVulkanDebugMessages(logs);
}

CDL_TEST_CASE(GeometryPool, LiveMeshesGetDistinctSlotsAndAddresses, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestGeometry gpu;
		const GPUMeshHandle first = CreateTriangle(gpu.Pool);
		const GPUMeshHandle second = CreateTriangle(gpu.Pool);

		CDL_EXPECT_NE(first.Index, second.Index);
		CDL_EXPECT_NE(gpu.Pool.GetMeshInfo(first).VerticesAddr, gpu.Pool.GetMeshInfo(second).VerticesAddr);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// A frame still in flight may draw a released mesh, so its buffers outlive the release until that frame retires.
CDL_TEST_CASE(GeometryPool, ReleaseFreesBuffersOnlyOnceTheFrameRetires, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestGeometry gpu;
		const GPUMeshHandle mesh = CreateTriangle(gpu.Pool);
		RunReleaseFrame(gpu);
		CDL_CHECK_EQ(LiveAllocations(gpu.Allocator), 2u);

		gpu.Pool.ReleaseMesh(mesh);
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 2u);

		RunReleaseFrame(gpu);
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 0u);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The generation is what tells a stale handle from the mesh that reused its slot.
CDL_TEST_CASE(GeometryPool, ReusesAReleasedSlotWithTheNextGeneration, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestGeometry gpu;
		const GPUMeshHandle released = CreateTriangle(gpu.Pool);
		gpu.Pool.ReleaseMesh(released);
		RunReleaseFrame(gpu);

		constexpr std::array<uint32_t, 6> quadIndices = { 0, 1, 2, 2, 1, 0 };
		const GPUMeshHandle reused = gpu.Pool.CreateMesh(kTriangleVertices, quadIndices);

		CDL_EXPECT_EQ(reused.Index, released.Index);
		CDL_EXPECT_EQ(reused.Generation, released.Generation + 1);
		CDL_EXPECT_EQ(gpu.Pool.GetMeshInfo(reused).IndexCount, 6u);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Until the render thread hands the slot back, it still holds the released mesh, so a new mesh takes another one.
CDL_TEST_CASE(GeometryPool, ReleasedSlotIsNotReusedBeforeItRetires, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestGeometry gpu;
		const GPUMeshHandle released = CreateTriangle(gpu.Pool);
		gpu.Pool.ReleaseMesh(released);

		const GPUMeshHandle created = CreateTriangle(gpu.Pool);
		CDL_EXPECT_NE(created.Index, released.Index);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The generation check: a handle dies when its mesh is released, before the GPU is done with it, and stays dead once
// another mesh takes its slot.
CDL_TEST_CASE(GeometryPool, HandlesDieWithTheirMesh, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestGeometry gpu;
		const GPUMeshHandle mesh = CreateTriangle(gpu.Pool);
		CDL_EXPECT(gpu.Pool.IsAlive(mesh));

		gpu.Pool.ReleaseMesh(mesh);
		CDL_EXPECT_FALSE(gpu.Pool.IsAlive(mesh));

		RunReleaseFrame(gpu);
		const GPUMeshHandle reused = CreateTriangle(gpu.Pool);
		CDL_CHECK_EQ(reused.Index, mesh.Index);
		CDL_EXPECT(gpu.Pool.IsAlive(reused));
		CDL_EXPECT_FALSE(gpu.Pool.IsAlive(mesh));
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The default handle, a slot past the end, and a generation the slot hasn't reached all miss.
CDL_TEST_CASE(GeometryPool, HandlesItNeverIssuedAreNotAlive, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestGeometry gpu;
		const GPUMeshHandle mesh = CreateTriangle(gpu.Pool);

		CDL_EXPECT_FALSE(gpu.Pool.IsAlive(GPUMeshHandle{}));
		CDL_EXPECT_FALSE(gpu.Pool.IsAlive(GPUMeshHandle{ .Index = mesh.Index + 1, .Generation = mesh.Generation }));
		CDL_EXPECT_FALSE(gpu.Pool.IsAlive(GPUMeshHandle{ .Index = mesh.Index, .Generation = mesh.Generation + 1 }));
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The free list is a stack: the slot recycled last is handed out first, and an unused slot is only taken once it is empty.
CDL_TEST_CASE(GeometryPool, HandsOutTheMostRecentlyFreedSlotFirst, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestGeometry gpu;
		const GPUMeshHandle first = CreateTriangle(gpu.Pool);
		const GPUMeshHandle second = CreateTriangle(gpu.Pool);
		const GPUMeshHandle third = CreateTriangle(gpu.Pool);
		gpu.Pool.ReleaseMesh(first);
		gpu.Pool.ReleaseMesh(third);
		RunReleaseFrame(gpu);

		CDL_EXPECT_EQ(CreateTriangle(gpu.Pool).Index, third.Index);
		CDL_EXPECT_EQ(CreateTriangle(gpu.Pool).Index, first.Index);

		const GPUMeshHandle grown = CreateTriangle(gpu.Pool);
		CDL_EXPECT_NE(grown.Index, first.Index);
		CDL_EXPECT_NE(grown.Index, second.Index);
		CDL_EXPECT_NE(grown.Index, third.Index);
	}
	ExpectNoVulkanDebugMessages(logs);
}
