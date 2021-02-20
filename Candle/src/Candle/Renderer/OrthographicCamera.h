#pragma once

#include "Candle/Core.h"

#include <glm/glm.hpp>

namespace Candle {

	class CANDLE_API OrthographicCamera
	{
	public:
		OrthographicCamera(float left, float right, float bottom, float top);
		~OrthographicCamera();

		inline void SetPosition(const glm::vec3& position) { _position = position; RecalculateViewMatrix(); }
		inline void SetRotation(float rotation) { _rotation = rotation; RecalculateViewMatrix(); }

		inline const glm::vec3& GetPosition() const { return _position; }
		inline float GetRotation() const { return _rotation; }

		inline const glm::mat4& GetProjectionMatrix() const { return _projectionMatrix; }
		inline const glm::mat4& GetViewMatrix() const { return _viewMatrix; }
		inline const glm::mat4& GetViewProjectionMatrix() const { return _viewProjectionMatrix; }

	private:
		void RecalculateViewMatrix();

	private:
		glm::mat4 _projectionMatrix;
		glm::mat4 _viewMatrix;
		glm::mat4 _viewProjectionMatrix;

		glm::vec3 _position;
		float _rotation = 0.0f;
	};
}