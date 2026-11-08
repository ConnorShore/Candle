#include "TestFramework.h"
#include "TestHelpers.h"

#include <Candle/Core/Memory/ScopedPtr.h>
#include <Candle/Core/Threading/Thread.h>
#include <Candle/Debug/ConsoleManager.h>
#include <Candle/Debug/DebugDrawManager.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <vector>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Unit;
using Candle::Test::Type::Stress;

namespace {

	constexpr glm::vec4 kWhite{ 1.0f };

	// The console outlives the manager, which holds a reference to it. Every test using this draws through DebugDraw, so it
	// skips where that's compiled out.
	struct TestDebugDraw
	{
		ConsoleManager Console;
		DebugDrawManager Manager{ Console };

		TestDebugDraw() { SkipUnlessDebugDrawEnabled(); }
	};

	void SubmitLine(float lineWidth, bool depthTest = true, float duration = 0.0f)
	{
		DebugDraw::Line(glm::vec3(0.0f), glm::vec3(1.0f), kWhite, { .Duration = duration, .LineWidth = lineWidth, .DepthTest = depthTest });
	}

}

CDL_TEST_CASE(DebugDrawManager, SplitsBatchesByLineWidth, Unit)
{
	TestDebugDraw test;
	SubmitLine(1.0f);
	SubmitLine(3.0f);
	SubmitLine(1.0f);

	DebugDrawData data;
	test.Manager.ExtractDebugDrawData(data, 0.0f);

	CDL_CHECK_EQ(data.Batches.size(), 2u);
	CDL_EXPECT_EQ(data.Batches[0].LineWidth, 1.0f);
	CDL_EXPECT_EQ(data.Batches[0].VertexCount, 4u);
	CDL_EXPECT_EQ(data.Batches[1].LineWidth, 3.0f);
	CDL_EXPECT_EQ(data.Batches[1].VertexCount, 2u);
}

CDL_TEST_CASE(DebugDrawManager, OrdersBatchesByBucketThenWidth, Unit)
{
	TestDebugDraw test;
	SubmitLine(1.0f, false);
	SubmitLine(3.0f, true);
	SubmitLine(1.0f, true);

	DebugDrawData data;
	test.Manager.ExtractDebugDrawData(data, 0.0f);

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
	TestDebugDraw test;
	DebugDraw::Cube(glm::vec3(0.0f), glm::vec3(1.0f), kWhite, { .LineWidth = 2.0f });
	SubmitLine(1.0f);
	DebugDraw::Point(glm::vec3(0.0f), kWhite, { .LineWidth = 2.0f, .DepthTest = false });

	DebugDrawData data;
	test.Manager.ExtractDebugDrawData(data, 0.0f);

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
	TestDebugDraw test;
	SubmitLine(2.0f);
	SubmitLine(2.05f);

	DebugDrawData data;
	test.Manager.ExtractDebugDrawData(data, 0.0f);

	CDL_CHECK_EQ(data.Batches.size(), 1u);
	CDL_EXPECT_EQ(data.Batches[0].LineWidth, 2.0f);
	CDL_EXPECT_EQ(data.Batches[0].VertexCount, 4u);
}

CDL_TEST_CASE(DebugDrawManager, ClampsZeroWidthToSmallestStep, Unit)
{
	TestDebugDraw test;
	SubmitLine(0.0f);

	DebugDrawData data;
	test.Manager.ExtractDebugDrawData(data, 0.0f);

	CDL_CHECK_EQ(data.Batches.size(), 1u);
	CDL_EXPECT_EQ(data.Batches[0].LineWidth, 1.0f / kLineWidthStepsPerPixel);
}

// The width is kept as a byte of steps, so without the clamp a wider line would wrap round to a thinner one
CDL_TEST_CASE(DebugDrawManager, ClampsWidthsPastTheLargestStep, Unit)
{
	TestDebugDraw test;
	SubmitLine(100.0f);

	DebugDrawData data;
	test.Manager.ExtractDebugDrawData(data, 0.0f);

	CDL_CHECK_EQ(data.Batches.size(), 1u);
	CDL_EXPECT_EQ(data.Batches[0].LineWidth, 255.0f / kLineWidthStepsPerPixel);
}

CDL_TEST_CASE(DebugDrawManager, PersistentEntriesKeepTheirWidth, Unit)
{
	TestDebugDraw test;
	SubmitLine(4.0f, true, 1.0f);
	SubmitLine(1.0f);

	DebugDrawData data;
	test.Manager.ExtractDebugDrawData(data, 0.0f);
	CDL_CHECK_EQ(data.Batches.size(), 2u);

	test.Manager.ExtractDebugDrawData(data, 0.5f);
	CDL_CHECK_EQ(data.Batches.size(), 1u);
	CDL_EXPECT_EQ(data.Batches[0].LineWidth, 4.0f);
	CDL_EXPECT_EQ(data.Vertices.size(), 2u);

	test.Manager.ExtractDebugDrawData(data, 0.5f);
	CDL_EXPECT(data.Batches.empty());
	CDL_EXPECT(data.Vertices.empty());
}

namespace {

	DebugDrawData ExtractOnce(DebugDrawManager& manager)
	{
		DebugDrawData data;
		manager.ExtractDebugDrawData(data, 0.0f);
		return data;
	}

	// The worst distance from center across every vertex, against radius, so one failure covers the whole shape
	float WorstRadiusError(const DebugDrawData& data, glm::vec3 center, float radius)
	{
		float worst = 0.0f;
		for (const GPU::DebugDrawVertex& vertex : data.Vertices)
			worst = std::max(worst, std::abs(glm::length(vertex.Position - center) - radius));
		return worst;
	}

	constexpr float kEpsilon = 1e-4f;

}

CDL_TEST_CASE(DebugDrawManager, LineKeepsItsEndpoints, Unit)
{
	TestDebugDraw test;
	DebugDraw::Line({ 1.0f, 2.0f, 3.0f }, { -4.0f, 5.0f, -6.0f }, kWhite);

	const DebugDrawData data = ExtractOnce(test.Manager);
	CDL_CHECK_EQ(data.Vertices.size(), 2u);
	CDL_EXPECT(data.Vertices[0].Position == glm::vec3(1.0f, 2.0f, 3.0f));
	CDL_EXPECT(data.Vertices[1].Position == glm::vec3(-4.0f, 5.0f, -6.0f));
}

CDL_TEST_CASE(DebugDrawManager, PointIsACrossOfItsSize, Unit)
{
	TestDebugDraw test;
	const glm::vec3 position{ 1.0f, 2.0f, 3.0f };
	DebugDraw::Point(position, 2.0f, kWhite);

	const DebugDrawData data = ExtractOnce(test.Manager);
	CDL_CHECK_EQ(data.Vertices.size(), 4u);
	CDL_EXPECT(data.Vertices[0].Position == position - glm::vec3(0.0f, 1.0f, 0.0f));
	CDL_EXPECT(data.Vertices[1].Position == position + glm::vec3(0.0f, 1.0f, 0.0f));
	CDL_EXPECT(data.Vertices[2].Position == position - glm::vec3(1.0f, 0.0f, 0.0f));
	CDL_EXPECT(data.Vertices[3].Position == position + glm::vec3(1.0f, 0.0f, 0.0f));
}

CDL_TEST_CASE(DebugDrawManager, PointWithoutASizeUsesTheDefault, Unit)
{
	TestDebugDraw test;
	DebugDraw::Point(glm::vec3(0.0f), kWhite);

	const DebugDrawData data = ExtractOnce(test.Manager);
	CDL_CHECK_EQ(data.Vertices.size(), 4u);
	CDL_EXPECT_NEAR(glm::length(data.Vertices[1].Position - data.Vertices[0].Position), kDefaultPointSize, kEpsilon);
}

// Segments chain end to start, and the last one ends where the first began
CDL_TEST_CASE(DebugDrawManager, CircleIsAClosedLoopOfSegments, Unit)
{
	TestDebugDraw test;
	DebugDraw::Circle(glm::vec3(0.0f), { 0.0f, 1.0f, 0.0f }, 1.0f, kWhite);

	const DebugDrawData data = ExtractOnce(test.Manager);
	CDL_CHECK_EQ(data.Vertices.size(), kCircleSegmentCount * 2);
	for (size_t i = 1; i + 1 < data.Vertices.size(); i += 2)
		CDL_EXPECT_NEAR(glm::length(data.Vertices[i].Position - data.Vertices[i + 1].Position), 0.0f, kEpsilon);
	CDL_EXPECT_NEAR(glm::length(data.Vertices.back().Position - data.Vertices.front().Position), 0.0f, kEpsilon);
}

CDL_TEST_CASE(DebugDrawManager, CircleLiesInThePlaneOfItsNormal, Unit)
{
	TestDebugDraw test;
	const glm::vec3 center{ 1.0f, 2.0f, 3.0f };
	const glm::vec3 normal = glm::normalize(glm::vec3(0.0f, 1.0f, 1.0f));
	DebugDraw::Circle(center, normal, 2.0f, kWhite);

	const DebugDrawData data = ExtractOnce(test.Manager);
	float worst = 0.0f;
	for (const GPU::DebugDrawVertex& vertex : data.Vertices)
		worst = std::max(worst, std::abs(glm::dot(vertex.Position - center, normal)));
	CDL_EXPECT_NEAR(worst, 0.0f, kEpsilon);
}

// Z takes the other reference axis in TessellateCircle's basis, and a tilted normal isn't perpendicular to either
CDL_TEST_CASE(DebugDrawManager, CircleKeepsItsRadiusForAnyNormal, Unit)
{
	const glm::vec3 center{ 1.0f, 2.0f, 3.0f };
	const std::array normals = { glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::normalize(glm::vec3(0.0f, 1.0f, 1.0f)) };
	for (const glm::vec3& normal : normals)
	{
		TestDebugDraw test;
		DebugDraw::Circle(center, normal, 2.0f, kWhite);

		CDL_NOTE(std::format("normal ({}, {}, {})", normal.x, normal.y, normal.z));
		CDL_EXPECT_NEAR(WorstRadiusError(ExtractOnce(test.Manager), center, 2.0f), 0.0f, kEpsilon);
	}
}

// Twelve edges, each joining two corners that differ along one axis by the cube's size on it
CDL_TEST_CASE(DebugDrawManager, CubeEdgesJoinItsCorners, Unit)
{
	TestDebugDraw test;
	const glm::vec3 center{ 1.0f, 2.0f, 3.0f };
	const glm::vec3 size{ 2.0f, 4.0f, 6.0f };
	DebugDraw::Cube(center, size, kWhite);

	const DebugDrawData data = ExtractOnce(test.Manager);
	CDL_CHECK_EQ(data.Vertices.size(), 24u);
	for (const GPU::DebugDrawVertex& vertex : data.Vertices)
		CDL_EXPECT(glm::abs(vertex.Position - center) == size * 0.5f);

	std::array<int, 3> edgesPerAxis{};
	for (size_t i = 0; i < data.Vertices.size(); i += 2)
	{
		const glm::vec3 edge = glm::abs(data.Vertices[i + 1].Position - data.Vertices[i].Position);
		const int axesMoved = (edge.x != 0.0f) + (edge.y != 0.0f) + (edge.z != 0.0f);
		CDL_CHECK_EQ(axesMoved, 1);

		const int axis = edge.x != 0.0f ? 0 : edge.y != 0.0f ? 1 : 2;
		CDL_EXPECT_EQ(edge[axis], size[axis]);
		++edgesPerAxis[axis];
	}
	CDL_EXPECT(edgesPerAxis == (std::array<int, 3>{ 4, 4, 4 }));
}

CDL_TEST_CASE(DebugDrawManager, SphereIsThreeCirclesAtItsRadius, Unit)
{
	TestDebugDraw test;
	const glm::vec3 center{ 1.0f, 2.0f, 3.0f };
	DebugDraw::Sphere(center, 2.0f, kWhite);

	const DebugDrawData data = ExtractOnce(test.Manager);
	CDL_CHECK_EQ(data.Vertices.size(), kCircleSegmentCount * 2 * 3);
	CDL_EXPECT_NEAR(WorstRadiusError(data, center, 2.0f), 0.0f, kEpsilon);
}

// Which nibble holds which channel is the shader's contract, which the Renderer tests check by drawing; these pin the range
CDL_TEST_CASE(DebugDrawManager, ColourPacksToFourBitsAChannelAndClamps, Unit)
{
	TestDebugDraw test;
	DebugDraw::Line(glm::vec3(0.0f), glm::vec3(1.0f), kWhite);
	DebugDraw::Line(glm::vec3(0.0f), glm::vec3(1.0f), glm::vec4(0.0f));
	DebugDraw::Line(glm::vec3(0.0f), glm::vec3(1.0f), glm::vec4(2.0f));
	DebugDraw::Line(glm::vec3(0.0f), glm::vec3(1.0f), glm::vec4(-1.0f));

	// The sort isn't stable, so entries sharing a key can come out in any order
	const DebugDrawData data = ExtractOnce(test.Manager);
	CDL_CHECK_EQ(data.Vertices.size(), 8u);
	CDL_EXPECT_EQ(std::ranges::count(data.Vertices, uint16_t{ 0xFFFF }, &GPU::DebugDrawVertex::Color), 4);
	CDL_EXPECT_EQ(std::ranges::count(data.Vertices, uint16_t{ 0x0000 }, &GPU::DebugDrawVertex::Color), 4);
}

CDL_TEST_CASE(DebugDrawManager, OneFrameEntriesDrawOnce, Unit)
{
	TestDebugDraw test;
	SubmitLine(1.0f);

	DebugDrawData data;
	test.Manager.ExtractDebugDrawData(data, 0.0f);
	CDL_EXPECT_EQ(data.Vertices.size(), 2u);

	test.Manager.ExtractDebugDrawData(data, 0.0f);
	CDL_EXPECT(data.Vertices.empty());
	CDL_EXPECT(data.Batches.empty());
}

CDL_TEST_CASE(DebugDrawManager, UnlimitedDurationNeverExpires, Unit)
{
	TestDebugDraw test;
	SubmitLine(1.0f, true, DebugDraw::kUnlimitedDuration);

	DebugDrawData data;
	for (int frame = 0; frame < 4; ++frame)
	{
		test.Manager.ExtractDebugDrawData(data, 1.0e6f);
		CDL_EXPECT_EQ(data.Vertices.size(), 2u);
	}
}

// The first frame's delta passed before the entry was submitted, so it doesn't count against the duration. The entry joins
// the persistent list after that list has been drawn, so it isn't drawn twice either.
CDL_TEST_CASE(DebugDrawManager, APersistentEntryDrawsOnceOnTheFrameItsSubmitted, Unit)
{
	TestDebugDraw test;
	SubmitLine(1.0f, true, 1.0f);

	DebugDrawData data;
	test.Manager.ExtractDebugDrawData(data, 5.0f);
	CDL_EXPECT_EQ(data.Vertices.size(), 2u);
}

// Expired entries are swap-removed, so the first expiring moves the last into its place. That one still needs counting down
// that frame, exactly once, and none of the others can be lost.
CDL_TEST_CASE(DebugDrawManager, ExpiringAPersistentEntryKeepsTheOthers, Unit)
{
	TestDebugDraw test;

	// Each line's width is its duration, so the batches say which entries are left
	SubmitLine(1.0f, true, 1.0f);
	SubmitLine(3.0f, true, 3.0f);
	SubmitLine(2.0f, true, 2.0f);

	const auto widthsLeft = [&](float delta) {
		DebugDrawData data;
		test.Manager.ExtractDebugDrawData(data, delta);
		std::vector<float> widths;
		for (const DebugDrawBatch& batch : data.Batches)
			widths.push_back(batch.LineWidth);
		return widths;
	};

	CDL_EXPECT(widthsLeft(0.0f) == (std::vector<float>{ 1.0f, 2.0f, 3.0f }));
	CDL_EXPECT(widthsLeft(1.0f) == (std::vector<float>{ 2.0f, 3.0f }));
	CDL_EXPECT(widthsLeft(1.0f) == (std::vector<float>{ 3.0f }));
	CDL_EXPECT(widthsLeft(1.0f).empty());
}

CDL_TEST_CASE(DebugDrawManager, NoDepthTestGoesToTheNoDepthBucket, Unit)
{
	TestDebugDraw test;
	SubmitLine(1.0f, false);

	const DebugDrawData data = ExtractOnce(test.Manager);
	CDL_CHECK_EQ(data.Batches.size(), 1u);
	CDL_EXPECT_EQ(data.Batches[0].Bucket, DebugDrawBucket::WorldNoDepth);
}

// Headless tools and code running before startup call these too, so with no manager the draw is dropped rather than crashing
CDL_TEST_CASE(DebugDrawManager, DrawsWithoutAManagerAreDropped, Unit)
{
	SubmitLine(1.0f);
	DebugDraw::Sphere(glm::vec3(0.0f), 1.0f, kWhite);

	TestDebugDraw test;
	CDL_EXPECT(ExtractOnce(test.Manager).Vertices.empty());
}

#if !CDL_DEBUG_DRAW_ENABLED
// Every other test here skips in Dist; this one checks the calls really are compiled out, so even a live manager gets nothing
CDL_TEST_CASE(DebugDrawManager, DrawsAreCompiledOut, Unit)
{
	ConsoleManager console;
	DebugDrawManager manager(console);
	SubmitLine(1.0f);
	DebugDraw::Sphere(glm::vec3(0.0f), 1.0f, kWhite);

	CDL_EXPECT(ExtractOnce(manager).Vertices.empty());
}
#endif

CDL_TEST_CASE(DebugDrawManager, AManagerCanBeReplacedOnceTheLastIsGone, Unit)
{
	{
		TestDebugDraw first;
		SubmitLine(1.0f);
	}

	TestDebugDraw second;
	SubmitLine(1.0f);
	CDL_EXPECT_EQ(ExtractOnce(second.Manager).Vertices.size(), 2u);
}

CDL_TEST_CASE(DebugDrawManager, SubmitsFromManyThreadsAreAllExtracted, Stress)
{
	constexpr int kThreads = 8;
	constexpr int kLinesPerThread = 10'000;

	TestDebugDraw test;
	{
		std::vector<ScopedPtr<Thread>> threads;
		for (int i = 0; i < kThreads; ++i)
		{
			threads.push_back(ScopedPtr<Thread>::Create("DebugDrawSubmitter"));
			threads.back()->Start([] {
				for (int line = 0; line < kLinesPerThread; ++line)
					SubmitLine(1.0f + static_cast<float>(line % 4));
			});
		}
		for (auto& thread : threads)
			thread->Join();
	}

	const DebugDrawData data = ExtractOnce(test.Manager);
	CDL_EXPECT_EQ(data.Vertices.size(), size_t{ kThreads } * kLinesPerThread * 2);
	CDL_EXPECT_EQ(data.Batches.size(), 4u);
}

// The main thread both submits and extracts, so a full queue has to drop rather than wait for room only that thread can make
CDL_TEST_CASE(DebugDrawManager, SubmitsPastTheQueueCapacityAreDropped, Stress)
{
	constexpr size_t kOverflow = 16;

	TestDebugDraw test;
	for (size_t i = 0; i < kDebugDrawEntryCount + kOverflow; ++i)
		SubmitLine(1.0f);
	CDL_EXPECT_EQ(ExtractOnce(test.Manager).Vertices.size(), kDebugDrawEntryCount * 2);

	// Extraction drained the queue, so there's room again
	SubmitLine(1.0f);
	CDL_EXPECT_EQ(ExtractOnce(test.Manager).Vertices.size(), 2u);
}
