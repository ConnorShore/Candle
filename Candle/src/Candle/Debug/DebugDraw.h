#pragma once

#include <glm/glm.hpp>

#include <limits>

#ifdef CDL_DIST
#define CDL_DEBUG_DRAW_ENABLED 0
#else
#define CDL_DEBUG_DRAW_ENABLED 1
#endif

namespace Candle::DebugDraw {

	constexpr float kUnlimitedDuration = std::numeric_limits<float>::max();

	struct Options
	{
		float Duration = 0.0f;  // Seconds; 0 draws for one frame
		bool DepthTest = true;
	};

#ifdef CDL_DEBUG_DRAW_ENABLED
	// Threading: any thread. No-ops when there's no manager: headless, before startup, after shutdown
	void Point(glm::vec3 position, glm::vec4 color, const Options& options = {});
	void Point(glm::vec3 position, float size, glm::vec4 color, const Options& options = {});
	void Line(glm::vec3 start, glm::vec3 end, glm::vec4 color, const Options& options = {});
	void Circle(glm::vec3 center, glm::vec3 planeNormal, float radius, glm::vec4 color, const Options& options = {});
	void Cube(glm::vec3 center, glm::vec3 size, glm::vec4 color, const Options& options = {});
	void Sphere(glm::vec3 center, float radius, glm::vec4 color, const Options& options = {});
#else
	inline void Point(glm::vec3, glm::vec4, const Options & = {}) {}
	inline void Point(glm::vec3, float, glm::vec4, const Options & = {}) {}
	inline void Line(glm::vec3, glm::vec3, glm::vec4, const Options & = {}) {}
	inline void Circle(glm::vec3, glm::vec3, float, glm::vec4, const Options & = {}) {}
	inline void Cube(glm::vec3, glm::vec3, glm::vec4, const Options & = {}) {}
	inline void Sphere(glm::vec3, float, glm::vec4, const Options & = {}) {}
#endif

}