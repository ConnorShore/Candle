#include "TestFramework.h"

#include <Candle/Render/Camera.h>

#include <cmath>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Unit;

// glm::perspective takes radians, so a default in degrees gives a 60 radian fov that still looks almost plausible
CDL_TEST_CASE(Camera, DefaultPerspectiveLensHasASixtyDegreeFov, Unit)
{
	const RenderView view = MakeRenderView(CameraPose{}, PerspectiveLens{}, 1.0f);
	CDL_EXPECT_NEAR(view.Projection[1][1], 1.0f / std::tan(glm::radians(30.0f)), 1e-5f);
}

CDL_TEST_CASE(Camera, PointAtHalfTheFovLandsOnTheTopEdge, Unit)
{
	const RenderView view = MakeRenderView(CameraPose{}, PerspectiveLens{}, 1.0f);

	// The camera looks down -Z, so this is 30 degrees above its forward axis
	const float distance = 10.0f;
	const glm::vec4 clip = view.Projection * view.View * glm::vec4(0.0f, distance * std::tan(glm::radians(30.0f)), -distance, 1.0f);
	CDL_EXPECT_NEAR(clip.y / clip.w, 1.0f, 1e-5f);
}
