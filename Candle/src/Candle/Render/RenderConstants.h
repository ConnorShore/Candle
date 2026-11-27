#pragma once

#include "Candle/Render/Vulkan/VulkanAPIVersion.h"

#include <cstdint>

namespace Candle {

	constexpr uint8_t kMaxFramesInFlight = 3;
	constexpr VulkanAPIVersion kDefaultVulkanVersion = VulkanAPIVersion::API_1_3;

	// Unique meshes resident at once, not instances: ~4K for a 5k-entity scene, times up to 4 LODs. Reserved up front, ~2 MB.
	constexpr uint32_t kMaxMeshes = 16 * 1024;
	constexpr uint32_t kMaxFrameGraphTextures = 512;
	constexpr uint32_t kMaxTextures = (16 * 1024) + kMaxFrameGraphTextures;
	constexpr uint32_t kMaxTransforms = 16 * 1024;		// Same size as meshes for now

	constexpr uint32_t kFrameDataArenaCapacity = 16 * 1024 * 1024;	// 16 MB (per frame) for the frame's data, transform data, etc, so the GPU can allocate from it instead of the CPU writing into a buffer

}