#include "Bindless.slangh"
#include "Candle/Render/Pipelines/TriangleInterop.h"

[[vk::push_constant]] ConstantBuffer<MeshDrawPushConstants> pushConstants;

struct VertexOutput {
    float4 position : SV_Position;
    float2 uv;
};

[shader("vertex")]
VertexOutput VertMain(uint vid : SV_VertexID) {
    VertexOutput output;
    Vertex vertex = pushConstants->Vertices[vid];
    float4x4 viewProj = pushConstants->Global.Frame->ViewProjection;
    float4x4 model = pushConstants->Global.Frame->TransformData[NonUniformResourceIndex(pushConstants->TransformIndex)];
    output.position = mul(viewProj, mul(model, float4(vertex.Position, 1.0)));
    output.uv = float2(vertex.UVx, 1.0 - vertex.UVy);
    return output;
}

[shader("fragment")]
float4 FragMain(VertexOutput inVert) : SV_Target
{
    // Each channel cycles at the same rate, a third of a turn apart
    float time = pushConstants->Global.Frame->Time;
    float4 baseColor = textures[pushConstants->TextureIndex].Sample(samplers[NonUniformResourceIndex(pushConstants->SamplerIndex)], inVert.uv);
	float4 computeColor = textures[pushConstants->ComputeTextureIndex].Sample(samplers[NonUniformResourceIndex(pushConstants->SamplerIndex)], inVert.uv);
	return baseColor + computeColor * 0.5;
    //return float4(baseColor.xyz * cos(time + float3(0.0, 2.0, 4.0)), 1.0);
}

[[vk::push_constant]] ConstantBuffer<ComputePushConstants> computePC;

[shader("compute")]
[numthreads(32, 32, 1)]
void CompMain(uint3 id : SV_DispatchThreadID)
{
	uint2 pixelCoord = id.xy;
	RWTexture2D<float4> target = storageTextures[computePC->TextureIndex];

	// The dispatch rounds up to whole groups, so the last ones can hang past the edge
	uint width, height;
	target.GetDimensions(width, height);
	if (pixelCoord.x >= width || pixelCoord.y >= height)
		return;

	// Written fresh each frame rather than scaling last frame's texel, which would decay to black
	float time = computePC->Global.Frame->Time;
	float3 wave = 0.5 + 0.5 * sin(time + float3(pixelCoord.x, pixelCoord.y, 0) * 0.1);
	target.Store(pixelCoord, float4(wave, 1.0));
}