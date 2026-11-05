#pragma once

#include "Candle/Render/Camera.h"

#include <glm/glm.hpp>

#include <vector>
#include <cstdint>

namespace Candle {

	// All temporary for now till we build out proper ECS, Scene, Entity and World

	struct Entity
	{
		uint64_t MeshID;
		uint64_t TextureID;
		glm::mat4 Transform;
	};

	struct World
	{
		std::vector<Entity> Entities;
		Candle::RenderView RenderView;
	};

}