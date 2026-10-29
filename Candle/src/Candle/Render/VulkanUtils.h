#pragma once

#include "RenderSpecification.h"

#include "Candle/Core/Core.h"
#include "Candle/Core/Version.h"
#include "Candle/Render/TextureData.h"

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

	// Every result is in the spec's mandatory set for sampled images with linear filtering, so it works on any device.
	inline vk::Format ToVulkanFormat(TextureFormat format, TextureColorSpace colorSpace)
	{
		const bool isSrgb = (colorSpace == TextureColorSpace::Srgb);
		switch (format)
		{
		case TextureFormat::Rgba8:
			return isSrgb ? vk::Format::eR8G8B8A8Srgb : vk::Format::eR8G8B8A8Unorm;
		case TextureFormat::Rg8:
			CDL_CORE_ASSERT(!isSrgb, "RG8 textures are linear only: R8G8_SRGB is optional, and few devices sample it");
			return vk::Format::eR8G8Unorm;
		case TextureFormat::R8:
			CDL_CORE_ASSERT(!isSrgb, "R8 textures are linear only: R8_SRGB is optional, and few devices sample it");
			return vk::Format::eR8Unorm;
		}

		CDL_CORE_ASSERT(false, "Unhandled TextureFormat");
		return vk::Format::eUndefined;
	}

}
