// The string helpers behind console command names and asset UUIDs.

#include "TestFramework.h"
#include "TestHelpers.h"

#include <Candle/Core/StringUtils.h>

#include <string>

using namespace Candle;
using Candle::Test::Type::Unit;

CDL_TEST_CASE(StringUtils, ToLowerLowersLettersAndNothingElse, Unit)
{
	CDL_EXPECT_EQ(ToLower("Debug.FRAME_2"), "debug.frame_2");
	CDL_EXPECT_EQ(ToLower("already lower"), "already lower");
	CDL_EXPECT_EQ(ToLower(""), "");
}

// Bytes above 127 are negative as plain chars, which std::tolower must never be handed
CDL_TEST_CASE(StringUtils, ToLowerLeavesNonAsciiBytesAlone, Unit)
{
	CDL_EXPECT_EQ(ToLower("\xC3\x89T\xC3\x89"), "\xC3\x89t\xC3\x89");
}

// AssetManager's UUIDs, so the hash has to come from the text and not from where it's stored
CDL_TEST_CASE(StringUtils, HashDependsOnlyOnTheText, Unit)
{
	const std::string owned = "Textures/Brick";

	CDL_EXPECT_EQ(HashStringToUUID(owned), HashStringToUUID("Textures/Brick"));
	CDL_EXPECT_NE(HashStringToUUID(owned), HashStringToUUID("textures/brick"));
	CDL_EXPECT_NE(HashStringToUUID(""), HashStringToUUID(" "));
}
