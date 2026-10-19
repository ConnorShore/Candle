#pragma once

#include <vulkan/vulkan.hpp>

namespace Candle {

	struct MeshDrawInfo
	{
		vk::DeviceAddress VerticesAddr;
		vk::Buffer Indices;
		uint32_t IndexCount;
	};

}