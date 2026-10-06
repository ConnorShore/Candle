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
