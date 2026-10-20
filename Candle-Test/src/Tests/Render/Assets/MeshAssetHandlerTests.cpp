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
		return SharedPtr<MeshAsset>::Create(MeshData{
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
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 2u);
	}
	ExpectNoVulkanDebugMessages(logs);
}

CDL_TEST_CASE(MeshAssetHandler, UnloadDestroysTheMeshAndClearsItsHandle, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestGeometry gpu;
		MeshAssetHandler handler(gpu.Pool);
		IAssetHandler& untyped = handler;
		SharedPtr<MeshAsset> asset = CreateTriangleAsset();
		untyped.OnLoad(*asset);

		untyped.OnUnload(*asset);

		CDL_EXPECT_FALSE(asset->GetGPUHandle().IsValid());
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

		assets.LoadAsset("Triangle", asset);

		CDL_EXPECT(asset->GetGPUHandle().IsValid());
		CDL_EXPECT(assets.GetAsset<MeshAsset>("Triangle") == asset);
	}
	ExpectNoVulkanDebugMessages(logs);
}
