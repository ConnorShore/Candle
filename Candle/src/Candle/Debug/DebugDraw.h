#pragma once

#include <glm/glm.hpp>

#ifdef CDL_DIST
#define CDL_DEBUG_DRAW_ENABLED 0
#else
#define CDL_DEBUG_DRAW_ENABLED 1
#endif

namespace Candle::DebugDraw {

	struct Options
	{
		float Duration = 0.0f;  // Seconds; 0 draws for one frame
		bool DepthTest = true;
	};

#ifdef CDL_DEBUG_DRAW_ENABLED
	// Threading: any thread. No-ops when there's no manager: headless, before startup, after shutdown
	void Cube(glm::vec3 center, glm::vec3 size, glm::vec4 color, const Options& options = {});
#else
	inline void Cube(glm::vec3, glm::vec3, glm::vec4, const Options & = {}) {}
#endif

}