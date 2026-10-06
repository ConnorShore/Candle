// VersionInfo: the dotted string, and the packed integer Vulkan receives as the application and engine version.

#include "TestFramework.h"

#include <Candle/Core/Version.h>

#include <cstdint>
#include <string>

using namespace Candle;
using Candle::Test::Type::Unit;

// VK_MAKE_API_VERSION's layout: major in bits 22-28, minor in 12-21, patch in 0-11.
CDL_TEST_CASE(Version, PacksLikeAVulkanApiVersion, Unit)
{
	CDL_EXPECT_EQ(static_cast<uint32_t>(VersionInfo{ 1, 2, 3 }), 0x00402003u);
	CDL_EXPECT_EQ(static_cast<uint32_t>(VersionInfo{ 0, 0, 0 }), 0u);
}

CDL_TEST_CASE(Version, ToStringIsDotted, Unit)
{
	const VersionInfo version{ 1, 2, 3 };
	CDL_EXPECT_EQ(version.ToString(), std::string("1.2.3"));
}
