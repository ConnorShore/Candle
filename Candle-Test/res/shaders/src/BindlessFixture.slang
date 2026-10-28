// Fixture for the texture tests: samples set 0 through the engine's own declaration of it, as Triangle.slang does, so a
// texture or sampler landing in the wrong element shows up as the wrong colour.

#include "../../../../Candle/res/shaders/src/Bindless.slangh"
#include "../../../src/Tests/Render/BindlessFixture.h"

[[vk::push_constant]] ConstantBuffer<BindlessFixturePushConstants> pushConstants;

struct VSOutput
{
    float4 position : SV_Position;
};

// One triangle covering the whole viewport, which the test shrinks to the one pixel a probe writes
[shader("vertex")]
VSOutput bindlessVertex(uint vertexId : SV_VertexID)
{
    VSOutput output;
    float2 corner = float2((vertexId << 1) & 2, vertexId & 2);
    output.position = float4(corner * 2.0 - 1.0, 0.0, 1.0);
    return output;
}

[shader("fragment")]
float4 bindlessFragment(VSOutput input) : SV_Target
{
    return textures[pushConstants->TextureIndex].Sample(samplers[pushConstants->SamplerIndex], pushConstants->UV);
}
