#include "Bindless.slangh"
#include "Candle/Render/GUI/GuiInterop.h"

[[vk::push_constant]] ConstantBuffer<GuiDrawPushConstants> pushConstants;

struct VertexOutput
{
	float4 position : SV_Position;
	float2 uv;
	float4 color;
};

// ImGui packs colours as RGBA8 with R in the low byte (IM_COL32 without IMGUI_USE_BGRA_PACKED_COLOR)
float4 UnpackColor(uint packed)
{
	return float4(packed & 0xFF, (packed >> 8) & 0xFF, (packed >> 16) & 0xFF, packed >> 24) / 255.0;
}

// ImGui's colours are sRGB, but the swapchain is an sRGB format that encodes on write, so they must be linear here or they
// get encoded twice and wash out. Alpha is coverage, never encoded
float3 SrgbToLinear(float3 color)
{
	return lerp(pow((color + 0.055) / 1.055, 2.4), color / 12.92, step(color, 0.04045));
}

[shader("vertex")]
VertexOutput VertMain(uint vid : SV_VertexID)
{
	VertexOutput output;
	GuiVertex vertex = pushConstants->Vertices[vid];
	output.uv = vertex.UV;

	float4 color = UnpackColor(vertex.Color);
	output.color = float4(SrgbToLinear(color.rgb), color.a);
	
    // Map screen-space UI coordinates to Vulkan clip-space (-1 to 1, with Y down)
	output.position = float4(vertex.Position * pushConstants->Scale + pushConstants->Translate, 0.0, 1.0);
	
	return output;
}

[shader("fragment")]
float4 FragMain(VertexOutput inVert) : SV_Target
{
	float4 baseColor = textures[pushConstants->TextureIndex].Sample(samplers[NonUniformResourceIndex(pushConstants->SamplerIndex)], inVert.uv);
	return inVert.color * baseColor;
}