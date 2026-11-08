#include "TestFramework.h"

#include <Candle/Debug/DebugDrawManager.h>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Unit;

namespace {

	constexpr glm::vec4 kWhite{ 1.0f };

	void SubmitLine(float lineWidth, bool depthTest = true, float duration = 0.0f)
	{
		DebugDraw::Line(glm::vec3(0.0f), glm::vec3(1.0f), kWhite, { .Duration = duration, .LineWidth = lineWidth, .DepthTest = depthTest });
	}

}

CDL_TEST_CASE(DebugDrawManager, SplitsBatchesByLineWidth, Unit)
{
	DebugDrawManager manager;
	SubmitLine(1.0f);
	SubmitLine(3.0f);
	SubmitLine(1.0f);

	DebugDrawData data;
	manager.ExtractDebugDrawData(data, 0.0f);

	CDL_CHECK_EQ(data.Batches.size(), 2u);
	CDL_EXPECT_EQ(data.Batches[0].LineWidth, 1.0f);
	CDL_EXPECT_EQ(data.Batches[0].VertexCount, 4u);
	CDL_EXPECT_EQ(data.Batches[1].LineWidth, 3.0f);
	CDL_EXPECT_EQ(data.Batches[1].VertexCount, 2u);
}

CDL_TEST_CASE(DebugDrawManager, OrdersBatchesByBucketThenWidth, Unit)
{
	DebugDrawManager manager;
	SubmitLine(1.0f, false);
	SubmitLine(3.0f, true);
	SubmitLine(1.0f, true);

	DebugDrawData data;
	manager.ExtractDebugDrawData(data, 0.0f);

	CDL_CHECK_EQ(data.Batches.size(), 3u);
	CDL_EXPECT_EQ(data.Batches[0].Bucket, DebugDrawBucket::WorldDepthTested);
	CDL_EXPECT_EQ(data.Batches[0].LineWidth, 1.0f);
	CDL_EXPECT_EQ(data.Batches[1].Bucket, DebugDrawBucket::WorldDepthTested);
	CDL_EXPECT_EQ(data.Batches[1].LineWidth, 3.0f);
	CDL_EXPECT_EQ(data.Batches[2].Bucket, DebugDrawBucket::WorldNoDepth);
	CDL_EXPECT_EQ(data.Batches[2].LineWidth, 1.0f);
}

CDL_TEST_CASE(DebugDrawManager, BatchesCoverVerticesBackToBack, Unit)
{
	DebugDrawManager manager;
	DebugDraw::Cube(glm::vec3(0.0f), glm::vec3(1.0f), kWhite, { .LineWidth = 2.0f });
	SubmitLine(1.0f);
	DebugDraw::Point(glm::vec3(0.0f), kWhite, { .LineWidth = 2.0f, .DepthTest = false });

	DebugDrawData data;
	manager.ExtractDebugDrawData(data, 0.0f);

	CDL_CHECK_EQ(data.Batches.size(), 3u);
	uint32_t nextVertex = 0;
	for (const DebugDrawBatch& batch : data.Batches)
	{
		CDL_EXPECT_EQ(batch.FirstVertex, nextVertex);
		nextVertex += batch.VertexCount;
	}
	CDL_EXPECT_EQ(static_cast<size_t>(nextVertex), data.Vertices.size());
}

CDL_TEST_CASE(DebugDrawManager, MergesWidthsWithinAQuantizationStep, Unit)
{
	DebugDrawManager manager;
	SubmitLine(2.0f);
	SubmitLine(2.05f);

	DebugDrawData data;
	manager.ExtractDebugDrawData(data, 0.0f);

	CDL_CHECK_EQ(data.Batches.size(), 1u);
	CDL_EXPECT_EQ(data.Batches[0].LineWidth, 2.0f);
	CDL_EXPECT_EQ(data.Batches[0].VertexCount, 4u);
}

CDL_TEST_CASE(DebugDrawManager, ClampsZeroWidthToSmallestStep, Unit)
{
	DebugDrawManager manager;
	SubmitLine(0.0f);

	DebugDrawData data;
	manager.ExtractDebugDrawData(data, 0.0f);

	CDL_CHECK_EQ(data.Batches.size(), 1u);
	CDL_EXPECT_EQ(data.Batches[0].LineWidth, 1.0f / kLineWidthStepsPerPixel);
}

CDL_TEST_CASE(DebugDrawManager, PersistentEntriesKeepTheirWidth, Unit)
{
	DebugDrawManager manager;
	SubmitLine(4.0f, true, 1.0f);
	SubmitLine(1.0f);

	DebugDrawData data;
	manager.ExtractDebugDrawData(data, 0.0f);
	CDL_CHECK_EQ(data.Batches.size(), 2u);

	manager.ExtractDebugDrawData(data, 0.5f);
	CDL_CHECK_EQ(data.Batches.size(), 1u);
	CDL_EXPECT_EQ(data.Batches[0].LineWidth, 4.0f);
	CDL_EXPECT_EQ(data.Vertices.size(), 2u);

	manager.ExtractDebugDrawData(data, 0.5f);
	CDL_EXPECT(data.Batches.empty());
	CDL_EXPECT(data.Vertices.empty());
}
