#include "Bindless.slangh"
#include "Candle/Render/Pipelines/DebugDrawInterop.h"

[[vk::push_constant]] ConstantBuffer<DebugDrawPushConstants> pushConstants;

struct VertexOutput
{
	float4 position : SV_Position;
	float4 color;
}

float4 UnpackColor(uint16_t packed)
{
	return float4(packed >> 12 & 0x0F,  (packed >> 8) & 0x0F, (packed >> 4) & 0x0F, packed & 0x0F) / 15.0;
}

[shader("vertex")]
VertexOutput VertMain(uint vid : SV_VertexID)
{
	VertexOutput output;
	DebugDrawVertex vertex = pushConstants->Vertices[vid];	// need to see if this is actually getting the right vertex with vid
	float4x4 viewProj = pushConstants->Global.Frame->ViewProjection;	// TODO: pass this somehow so its either a world or screen space matrix
	output.position = mul(viewProj, float4(vertex.Position, 1.0));
	output.color = UnpackColor(vertex.Color);
	return output;
}

[shader("fragment")]
float4 FragMain(VertexOutput inVert) : SV_Target
{
	return inVert.color;
}