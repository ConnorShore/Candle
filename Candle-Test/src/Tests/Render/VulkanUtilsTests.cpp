// VulkanUtils: conversions from engine types to Vulkan's. Headers only, so Unit: no driver needed.

#include "TestFramework.h"

#include <Candle/Render/VulkanUtils.h>

using namespace Candle;
using Candle::Test::Type::Unit;

CDL_TEST_CASE(VulkanUtils, ToVulkanApiVersionMapsEachVersion, Unit)
{
	CDL_EXPECT_EQ(ToVulkanApiVersion(VulkanAPIVersion::API_1_3), static_cast<uint32_t>(VK_API_VERSION_1_3));
	CDL_EXPECT_EQ(ToVulkanApiVersion(VulkanAPIVersion::API_1_4), static_cast<uint32_t>(VK_API_VERSION_1_4));
}

// VK_MAKE_API_VERSION's layout: major in bits 22-28, minor in 12-21, patch in 0-11.
CDL_TEST_CASE(VulkanUtils, ToVulkanVersionPacksLikeVkMakeApiVersion, Unit)
{
	CDL_EXPECT_EQ(ToVulkanVersion(VersionInfo{ 1, 2, 3 }), 0x00402003u);
	CDL_EXPECT_EQ(ToVulkanVersion(VersionInfo{ 0, 0, 0 }), 0u);
	CDL_EXPECT_EQ(ToVulkanVersion(VersionInfo{ 127, 1023, 4095 }), 0x1FFFFFFFu);	// Every field at its widest, none spilling
}

// sRGB picks the format whose sampler decodes to linear; linear stores the bytes as they are, for data like normals.
CDL_TEST_CASE(VulkanUtils, ToVulkanFormatPicksTheColorSpacesVariantOfRgba8, Unit)
{
	CDL_EXPECT(ToVulkanFormat(TextureFormat::eRgba8, TextureColorSpace::eSrgb) == vk::Format::eR8G8B8A8Srgb);
	CDL_EXPECT(ToVulkanFormat(TextureFormat::eRgba8, TextureColorSpace::eLinear) == vk::Format::eR8G8B8A8Unorm);
}

// Their sRGB variants are optional, so asking for one asserts; that path can't run here, since asserts break.
CDL_TEST_CASE(VulkanUtils, ToVulkanFormatMapsDataFormatsToUnorm, Unit)
{
	CDL_EXPECT(ToVulkanFormat(TextureFormat::eRg8, TextureColorSpace::eLinear) == vk::Format::eR8G8Unorm);
	CDL_EXPECT(ToVulkanFormat(TextureFormat::eR8, TextureColorSpace::eLinear) == vk::Format::eR8Unorm);
}
