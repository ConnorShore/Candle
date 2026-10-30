#pragma once

#include "Candle/Render/Vulkan/VulkanAPIVersion.h"

#include <cstdint>

namespace Candle {

	constexpr uint8_t kMaxFramesInFlight = 3;
	constexpr VulkanAPIVersion kDefaultVulkanVersion = VulkanAPIVersion::API_1_3;

	// Unique meshes resident at once, not instances: ~4K for a 5k-entity scene, times up to 4 LODs. Reserved up front, ~2 MB.
	constexpr uint32_t kMaxMeshes = 16 * 1024;
	constexpr uint32_t kMaxTextures = 16 * 1024;
	constexpr uint32_t kMaxTransforms = 16 * 1024; // Same size as meshes for now
}