#pragma once

// Compiled as C++ by ShaderInteropTests and as Slang by Candle-Test/res/shaders/src/ShaderInteropFixture.slang,
// which includes it by relative path. Changing it means recompiling the fixture with Build.py shaders.

#include "Candle/Render/ShaderInterop/GlobalPushConstants.h"
#include "Candle/Render/ShaderInterop/Vertex.h"

CDL_INTEROP_BEGIN

	// Shaped like a production block: Global, then per-draw geometry and material values. Frame-wide and variable-count
	// data stays behind Global.Frame, never embedded, which is what keeps a block within the 128-byte range.
	// The material members sit where C layout and std430 disagree, so a shader compiled without C layout fails to match.
	struct InteropFixturePushConstants
	{
		GlobalPushConstants Global;
		CDL_GPU_PTR(Vertex) Vertices;
		uint VertexOffset;				// Into Vertices, as for meshes sharing one buffer
		uint TransformIndex;			// Into Global.Frame->TransformData
		float3 EmissiveColor;			// std430 aligns a float3 to 16: 32, not 24
		float EmissiveStrength;			// Packs into the float3's tail in both layouts, so it moves with it
		float4 BaseColorFactor;			// std430 aligns a float4 to 16: 48, not 40
		float2 UVScale;
	};

CDL_INTEROP_END
