// GPUMeshHandle's validity, which callers check before handing a handle to GeometryPool.

#include "TestFramework.h"

#include <Candle/Render/GPUHandles.h>

using namespace Candle;
using Candle::Test::Type::Unit;

CDL_TEST_CASE(GPUHandles, DefaultMeshHandleIsInvalid, Unit)
{
	const GPUMeshHandle handle;
	CDL_EXPECT_FALSE(handle.IsValid());
	CDL_EXPECT_EQ(handle.Generation, 0u);
}

// Index 0 with generation 0 is the first slot's first mesh, not a null handle.
CDL_TEST_CASE(GPUHandles, AnyIndexButTheSentinelIsValid, Unit)
{
	CDL_EXPECT((GPUMeshHandle{ .Index = 0, .Generation = 0 }.IsValid()));
	CDL_EXPECT((GPUMeshHandle{ .Index = 41, .Generation = 3 }.IsValid()));
}
