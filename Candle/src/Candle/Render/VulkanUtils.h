#pragma once

#include "RenderSpecification.h"

#include "Candle/Core/Core.h"
#include "Candle/Core/Version.h"

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

	// Vulkan packs major into 7 bits, minor into 10 and patch into 12; masked so an oversized field can't spill into its neighbour.
	inline uint32_t ToVulkanVersion(const VersionInfo& version)
	{
		CDL_CORE_ASSERT(version.Major >= 0 && version.Major < (1 << 7)
			&& version.Minor >= 0 && version.Minor < (1 << 10)
			&& version.Patch >= 0 && version.Patch < (1 << 12), "VersionInfo field out of range for Vulkan's packing");

		return vk::makeApiVersion(0u,
			static_cast<uint32_t>(version.Major) & 0x7Fu,
			static_cast<uint32_t>(version.Minor) & 0x3FFu,
			static_cast<uint32_t>(version.Patch) & 0xFFFu);
	}

}
