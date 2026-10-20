#pragma once

// Compiled as C++ by ShaderInteropTests and as Slang by Candle-Test/res/shaders/src/ShaderInteropFixture.slang,
// which includes it by relative path. Changing it means recompiling the fixture with Build.py shaders.

#include "Candle/Render/ShaderInterop/GlobalPushConstants.h"
#include "Candle/Render/ShaderInterop/Vertex.h"

CDL_INTEROP_BEGIN

	// Members placed where C++ and std430 disagree, so a shader compiled without C layout fails to match.
	// Starts with Global, so Pipeline accepts it as a push-constant block.
	struct InteropFixturePushConstants
	{
		GlobalPushConstants Global;
		float Scalar;
		float3 Vector3;					// std430 aligns a float3 to 16
		float4 Vector4;					// std430 aligns a float4 to 16
		uint Count;
		float2 Vector2;					// std430 aligns a float2 to 8
		Vertex EmbeddedVertex;			// A struct of one float3: 16-aligned and 16 bytes in std430
		FrameData EmbeddedFrame;
		CDL_GPU_PTR(Vertex) Vertices;
	};

CDL_INTEROP_END
