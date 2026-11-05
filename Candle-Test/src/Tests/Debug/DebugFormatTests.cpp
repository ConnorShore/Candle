// The byte formatting the debug panels share.

#include "TestFramework.h"
#include "TestHelpers.h"

#include <Candle/Debug/DebugFormat.h>

using namespace Candle;
using Candle::Test::Type::Unit;

CDL_TEST_CASE(DebugFormat, FormatBytesPicksTheLargestWholeUnit, Unit)
{
	CDL_EXPECT_EQ(FormatBytes(0), "0 B");
	CDL_EXPECT_EQ(FormatBytes(1023), "1023 B");
	CDL_EXPECT_EQ(FormatBytes(1024), "1.0 KB");
	CDL_EXPECT_EQ(FormatBytes(1024 * 1024), "1.0 MB");
	CDL_EXPECT_EQ(FormatBytes(1024 * 1024 * 1024), "1.00 GB");
	CDL_EXPECT_EQ(FormatBytes(16ull * 1024 * 1024 * 1024), "16.00 GB");
}
