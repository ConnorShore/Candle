#pragma once

#include "Candle/Core/Time.h"
#include "Candle/Input/InputSnapshot.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <variant>

namespace Candle {

	// TODO: This Camera code is all very placeholder for now. This is how it changes in the future
	//	1. Lens will belong in a Camera Component
	//  2. CameraPose will belong in a Transform Component
	//  3. Controllers also become a component and are driven by a system

	struct PerspectiveLens 
	{ 
		float VerticalFov{ glm::radians(60.0f) };
		float Near{ 0.1f };
		float Far{ 100.0f };
	};
	struct OrthographicLens 
	{ 
		float Height{ 10.0f }; 
		float Near{ 0.1f }; 
		float Far{ 100.0f }; 
	};
	using CameraLens = std::variant<PerspectiveLens, OrthographicLens>;

	struct CameraPose 
	{ 
		glm::vec3 Position{ 0.0f };
		glm::quat Orientation{ 1.0f, 0.0f, 0.0f, 0.0f };
	};

	// Carried by value in FramePacket; the render thread never sees a camera
	struct RenderView 
	{ 
		glm::mat4 View{ 1.0f };
		glm::mat4 Projection{ 1.0f };
		glm::vec3 Position{ 0.0f };
	};

	RenderView MakeRenderView(const CameraPose& pose, const CameraLens& lens, float aspect);

	struct SimpleController { float MoveSpeed; float LookSpeed; };
	CameraPose UpdateSimpleController(const SimpleController& controller, CameraPose pose, const InputSnapshot& input, TimeStep delta);

	//struct OrbitController { glm::vec3 Focus; float Distance; float Yaw; float Pitch; };
	//CameraPose UpdateOrbit(OrbitController& orbit, const InputSnapshot& input, TimeStep delta);

}