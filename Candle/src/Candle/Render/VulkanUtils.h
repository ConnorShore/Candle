s#pragma once

#include "RenderSpecification.h"
#include "Candle/Core/Core.h"

#include <vulkan/vulkan.hpp>

#include <cstdint>

namespace Candle {

	inline uint32_t ToVulkanApiVersion(VulkanAPIVersion version)
	{
		switch (version)
		{
			case VulkanAPIVersion::API_1_3: return vk::ApiVersion13;
			case VulkanAPIVersion::API_1_4: return vk::ApiVersion14;
		}

		CDL_CORE_ASSERT(false, "Unhandled VulkanAPIVersion");
		return vk::ApiVersion13;
	}

}
