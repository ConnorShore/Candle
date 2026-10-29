// Fixture for ShaderInteropTests, ShaderLoaderTests and PipelineTests: a vertex and fragment pair sharing one push block
// defined in a C++/Slang interop header, which the tests compare against this module's reflection.
// Reads its data as a production mesh shader does: per-draw values from the push block, frame data through Global.Frame.

#include "../../../src/Tests/Render/ShaderInteropFixture.h"

[[vk::push_constant]] ConstantBuffer<InteropFixturePushConstants> pushConstants;

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv;
};

[shader("vertex")]
VSOutput interopVertex(uint vertexId : SV_VertexID)
{
    FrameData* frame = pushConstants->Global.Frame;
    Vertex vertex = pushConstants->Vertices[pushConstants->VertexOffset + vertexId];
    float4x4 model = frame->TransformData[pushConstants->TransformIndex];

    VSOutput output;
    output.position = mul(frame->ViewProjection, mul(model, float4(vertex.Position, 1.0)));
    output.uv = float2(vertex.UVx, vertex.UVy) * pushConstants->UVScale;
    return output;
}

[shader("fragment")]
float4 interopFragment(VSOutput input) : SV_Target
{
    float3 emissive = pushConstants->EmissiveColor * pushConstants->EmissiveStrength * pushConstants->Global.Frame->Time;
    return pushConstants->BaseColorFactor * float4(input.uv, 1.0, 1.0) + float4(emissive, 0.0);
}
