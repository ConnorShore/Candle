#include "cdlpch.h"
#include "Camera.h"

namespace Candle {

	RenderView MakeRenderView(const CameraPose& pose, const CameraLens& lens, float aspect)
	{
		const glm::mat4 projection = std::visit(Overloaded{
			[&](const PerspectiveLens& p) 
			{ 
				return glm::perspective(p.VerticalFov, aspect, p.Near, p.Far); 
			},
			[&](const OrthographicLens& o)
			{
				const float halfHeight = o.Height * 0.5f;
				return glm::ortho(-halfHeight * aspect, halfHeight * aspect, -halfHeight, halfHeight, o.Near, o.Far);
			}
		}, lens);

		return RenderView{
			.View = glm::mat4_cast(glm::conjugate(pose.Orientation)) * glm::translate(glm::mat4(1.0f), -pose.Position),
			.Projection = projection,
			.Position = pose.Position
		};
	}

	CameraPose UpdateSimpleController(const SimpleController& controller, CameraPose pose, const InputSnapshot& input, TimeStep delta)
	{
		CameraPose newPose = pose;

		// WASD movement
		if (input.IsKeyDown(KeyCode::W))
			newPose.Position += glm::vec3(newPose.Orientation * glm::vec3(0.0f, 0.0f, -1.0f)) * controller.MoveSpeed * delta.GetSeconds();
		if (input.IsKeyDown(KeyCode::S))
			newPose.Position += glm::vec3(newPose.Orientation * glm::vec3(0.0f, 0.0f, 1.0f)) * controller.MoveSpeed * delta.GetSeconds();
		if (input.IsKeyDown(KeyCode::A))
			newPose.Position += glm::vec3(newPose.Orientation * glm::vec3(-1.0f, 0.0f, 0.0f)) * controller.MoveSpeed * delta.GetSeconds();
		if (input.IsKeyDown(KeyCode::D))
			newPose.Position += glm::vec3(newPose.Orientation * glm::vec3(1.0f, 0.0f, 0.0f)) * controller.MoveSpeed * delta.GetSeconds();
		
		// Hold right mouse button to look around
		if (input.IsMouseDown(MouseButton::Right))
		{
			const glm::vec2 mouseDelta = input.MouseDelta;
			const float yaw = -mouseDelta.x * controller.LookSpeed;
			const float pitch = -mouseDelta.y * controller.LookSpeed;

			// Yaw around world up and pitch around the camera's own right axis, or the horizon rolls
			const glm::quat yawRotation = glm::angleAxis(glm::radians(yaw), glm::vec3(0.0f, 1.0f, 0.0f));
			const glm::quat pitchRotation = glm::angleAxis(glm::radians(pitch), glm::vec3(1.0f, 0.0f, 0.0f));
			newPose.Orientation = glm::normalize(yawRotation * newPose.Orientation * pitchRotation);
		}
		return newPose;
	}

}