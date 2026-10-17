#pragma once

#include "VulkanAPIVersion.h"

#include <cstdint>

namespace Candle {

	constexpr uint8_t kMaxFramesInFlight = 3;
	constexpr VulkanAPIVersion kDefaultVulkanVersion = VulkanAPIVersion::API_1_3;

}