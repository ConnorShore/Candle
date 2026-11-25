#include "Bindless.slangh"
#include "Candle/Render/Pipelines/FrameGraphPreviewInterop.h"

[[vk::push_constant]] ConstantBuffer<FrameGraphPreviewPushConstants> pushConstants;

struct VertexOutput
{
	float4 position : SV_Position;
	float2 uv;
};

// One triangle over the whole target, with UV 0..1 across the part inside it
[shader("vertex")]
VertexOutput VertMain(uint vid : SV_VertexID)
{
	VertexOutput output;
	output.uv = float2((vid << 1) & 2, vid & 2);
	output.position = float4(output.uv * 2.0 - 1.0, 0.0, 1.0);
	return output;
}

[shader("fragment")]
float4 FragMain(VertexOutput inVert) : SV_Target
{
	float4 value = textures[pushConstants->TextureIndex].Sample(samplers[NonUniformResourceIndex(pushConstants->SamplerIndex)], inVert.uv);
	float2 range = pushConstants->Range;
	float4 remapped = saturate((value - range.x) / max(range.y - range.x, 1e-6));

	// Depth only fills red, so it's shown as grey
	if (pushConstants->IsDepth != 0)
		return float4(remapped.rrr, 1.0);

	// Opaque, or the GUI's alpha blend would let the window behind it show through
	return float4(remapped.rgb, 1.0);
}
