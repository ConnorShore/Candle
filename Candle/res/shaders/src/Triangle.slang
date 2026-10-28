#include "Bindless.slangh"
#include "Candle/Render/Pipelines/TriangleInterop.h"

[[vk::push_constant]] ConstantBuffer<MeshDrawPushConstants> pushConstants;

struct VertexOutput {
    float4 sv_position : SV_Position;
    float2 uv;
};

[shader("vertex")]
VertexOutput VertMain(uint vid : SV_VertexID) {
    VertexOutput output;
    Vertex vertex = pushConstants->Vertices[vid];
    output.sv_position = float4(vertex.Position, 1.0);
    output.uv = float2(vertex.UVx, 1.0 - vertex.UVy);
    return output;
}

[shader("fragment")]
float4 FragMain(VertexOutput inVert) : SV_Target
{
    // Each channel cycles at the same rate, a third of a turn apart
    float time = pushConstants->Global.Frame->Time;
    float4 baseColor = textures[pushConstants->TextureIndex].Sample(samplers[pushConstants->SamplerIndex], inVert.uv);
    return float4(baseColor.xyz * cos(time + float3(0.0, 2.0, 4.0)), 1.0);
}