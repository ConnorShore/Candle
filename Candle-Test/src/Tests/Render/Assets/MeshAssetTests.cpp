// MeshAsset before any handler has seen it: its type, its CPU-side data, and no GPU mesh yet.

#include "TestFramework.h"

#include <Candle/Asset/Asset.h>
#include <Candle/Render/Assets/MeshAsset.h>
#include <Candle/Render/Assets/MeshData.h>

#include <cstdint>
#include <vector>

using namespace Candle;
using Candle::Test::Type::Unit;

CDL_STATIC_ASSERT(IsAsset<MeshAsset>);

namespace {

	constexpr const char* kTriangleMeshName = "TriangleMesh";

	MeshData TriangleData()
	{
		return {
			.Vertices = { { { 0.5f, -0.5f, 0.0f } }, { { 0.0f, 0.5f, 0.0f } }, { { -0.5f, -0.5f, 0.0f } } },
			.Indices = { 0, 1, 2 } };
	}

}

CDL_TEST_CASE(MeshAsset, IsAMeshAsset, Unit)
{
	const MeshAsset asset(kTriangleMeshName, TriangleData());

	CDL_EXPECT_EQ(MeshAsset::kType, AssetType::Mesh);
	CDL_EXPECT_EQ(asset.GetType(), AssetType::Mesh);
}

CDL_TEST_CASE(MeshAsset, KeepsItsMeshDataAndStartsWithoutAGPUMesh, Unit)
{
	const MeshAsset asset(kTriangleMeshName, TriangleData());
	const MeshData& data = asset.GetMeshData();

	CDL_CHECK_EQ(data.Vertices.size(), 3u);
	CDL_EXPECT(data.Vertices[1].Position == glm::vec3(0.0f, 0.5f, 0.0f));
	CDL_EXPECT(data.Indices == std::vector<uint32_t>({ 0, 1, 2 }));
	CDL_EXPECT_FALSE(asset.GetGPUHandle().IsValid());
}
