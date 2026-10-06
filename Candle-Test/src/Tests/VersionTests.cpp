// VersionInfo: the dotted string. Packing for Vulkan is ToVulkanVersion, tested with VulkanUtils.

#include "TestFramework.h"

#include <Candle/Core/Version.h>

#include <string>

using namespace Candle;
using Candle::Test::Type::Unit;

CDL_TEST_CASE(Version, ToStringIsDotted, Unit)
{
	const VersionInfo version{ 1, 2, 3 };
	CDL_EXPECT_EQ(version.ToString(), std::string("1.2.3"));
}
