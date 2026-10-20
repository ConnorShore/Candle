// Fixture for ShaderInteropTests, ShaderLoaderTests and PipelineTests: a vertex and fragment pair sharing one push block
// defined in a C++/Slang interop header, which the tests compare against this module's reflection.

#include "../../../src/Tests/Render/ShaderInteropFixture.h"

[[vk::push_constant]] ConstantBuffer<InteropFixturePushConstants> pushConstants;

struct VSOutput
{
    float4 position : SV_Position;
};

[shader("vertex")]
VSOutput interopVertex(uint vertexId : SV_VertexID)
{
    VSOutput output;
    float3 position = pushConstants->Vertices[vertexId].Position + pushConstants->Vector3 * pushConstants->Scalar;
    output.position = float4(position, 1.0);
    return output;
}

[shader("fragment")]
float4 interopFragment(VSOutput input) : SV_Target
{
    return pushConstants->Vector4 * pushConstants->Global.Frame->Time;
}
