#include "Candle/Render/Pipelines/TriangleInterop.h"

[[vk::push_constant]] ConstantBuffer<MeshDrawPushConstants> pushConstants;

struct VertexOutput {
    float4 sv_position : SV_Position;
};

[shader("vertex")]
VertexOutput VertMain(uint vid : SV_VertexID) {
    VertexOutput output;
    Vertex vertex = pushConstants->Vertices[vid];
    output.sv_position = float4(vertex.Position, 1.0);
    return output;
}

[shader("fragment")]
float4 FragMain(VertexOutput inVert) : SV_Target
{
    // Each channel cycles at the same rate, a third of a turn apart
    float time = pushConstants->Global.Frame->Time;
    return float4(0.5 + 0.5 * cos(time + float3(0.0, 2.0, 4.0)), 1.0);
}