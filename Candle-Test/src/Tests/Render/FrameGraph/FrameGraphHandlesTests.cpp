// A handle names one version of one texture. IsSameResource asks only about the texture and == about the version too,
// which is what lets the builder tell a handle from before a write from the one the write handed back.

#include "TestFramework.h"

#include <Candle/Render/FrameGraph/FrameGraphHandles.h>

using namespace Candle;
using Candle::Test::Type::Unit;

CDL_TEST_CASE(FrameGraphHandles, DefaultHandleIsInvalid, Unit)
{
	CDL_EXPECT_FALSE(FGTextureHandle{}.IsValid());
	CDL_EXPECT(FGTextureHandle{ .Index = 0 }.IsValid());
}

CDL_TEST_CASE(FrameGraphHandles, VersionsOfOneTextureAreTheSameResourceButNotEqual, Unit)
{
	const FGTextureHandle beforeWrite{ .Index = 3, .Version = 1 };
	const FGTextureHandle afterWrite{ .Index = 3, .Version = 2 };

	CDL_EXPECT(beforeWrite.IsSameResource(afterWrite));
	CDL_EXPECT_FALSE(beforeWrite == afterWrite);
	CDL_EXPECT((afterWrite == FGTextureHandle{ .Index = 3, .Version = 2 }));
}

CDL_TEST_CASE(FrameGraphHandles, DifferentTexturesAtTheSameVersionAreNeither, Unit)
{
	const FGTextureHandle color{ .Index = 1, .Version = 1 };
	const FGTextureHandle depth{ .Index = 2, .Version = 1 };

	CDL_EXPECT_FALSE(color.IsSameResource(depth));
	CDL_EXPECT_FALSE(color == depth);
}
