// MeshAssetHandler uploading into a real GeometryPool, called directly and through AssetManager.
// Each test fails on any validation message.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "../RenderTestHelpers.h"

#include <Candle/Asset/AssetManager.h>
#include <Candle/Render/Assets/MeshAsset.h>
#include <Candle/Render/Assets/MeshAssetHandler.h>

#include <vector>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Gpu;

namespace {

	SharedPtr<MeshAsset> CreateTriangleAsset()
	{
		return SharedPtr<MeshAsset>::Create("Triangle", MeshData{
			.Vertices = { kTriangleVertices.begin(), kTriangleVertices.end() },
			.Indices = { kTriangleIndices.begin(), kTriangleIndices.end() } });
	}

}

CDL_TEST_CASE(MeshAssetHandler, LoadUploadsTheMeshAndStoresItsHandle, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestGeometry gpu;
		MeshAssetHandler handler(gpu.Pool);
		IAssetHandler& untyped = handler;
		SharedPtr<MeshAsset> asset = CreateTriangleAsset();

		CDL_EXPECT_EQ(untyped.GetAssetType(), AssetType::Mesh);
		untyped.OnLoad(*asset);

		CDL_CHECK(asset->GetGPUHandle().IsValid());
		CDL_EXPECT_EQ(gpu.Pool.GetMeshInfo(asset->GetGPUHandle()).IndexCount, 3u);

		RunReleaseFrame(gpu);
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 2u);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The handle is cleared at once; the buffers go when the frame that carried the release retires.
CDL_TEST_CASE(MeshAssetHandler, UnloadReleasesTheMeshAndClearsItsHandle, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestGeometry gpu;
		MeshAssetHandler handler(gpu.Pool);
		IAssetHandler& untyped = handler;
		SharedPtr<MeshAsset> asset = CreateTriangleAsset();
		untyped.OnLoad(*asset);
		const GPUMeshHandle mesh = asset->GetGPUHandle();

		untyped.OnUnload(*asset);

		CDL_EXPECT_FALSE(asset->GetGPUHandle().IsValid());
		CDL_EXPECT_FALSE(gpu.Pool.IsAlive(mesh));

		RunReleaseFrame(gpu);
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 0u);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// RenderManager registers this handler with the application's AssetManager; this is that path without a window.
CDL_TEST_CASE(MeshAssetHandler, AssetManagerLoadsMeshesThroughIt, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestGeometry gpu;
		AssetManager assets;	// After the pool its handler uploads into, so it is destroyed first
		assets.RegisterAssetHandler(ScopedPtr<MeshAssetHandler>::Create(gpu.Pool));
		SharedPtr<MeshAsset> asset = CreateTriangleAsset();

		assets.LoadAsset(asset);

		CDL_EXPECT(asset->GetGPUHandle().IsValid());
		CDL_EXPECT(assets.GetAsset<MeshAsset>("Triangle") == asset);
	}
	ExpectNoVulkanDebugMessages(logs);
}

CDL_TEST_CASE(MeshAssetHandler, AssetManagerUnloadsMeshesThroughIt, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestGeometry gpu;
		AssetManager assets;
		assets.RegisterAssetHandler(ScopedPtr<MeshAssetHandler>::Create(gpu.Pool));
		SharedPtr<MeshAsset> asset = CreateTriangleAsset();
		assets.LoadAsset(asset);

		assets.UnloadAsset("Triangle", AssetType::Mesh);

		CDL_EXPECT_FALSE(asset->GetGPUHandle().IsValid());
		CDL_EXPECT(assets.GetAsset<MeshAsset>("Triangle") == nullptr);

		RunReleaseFrame(gpu);
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 0u);
	}
	ExpectNoVulkanDebugMessages(logs);
}
