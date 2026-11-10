// Fixture for the texture tests: samples and stores through the engine's own declaration of set 0, as Triangle.slang does,
// so a texture, storage image or sampler landing in the wrong element shows up as the wrong colour.

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

[[vk::push_constant]] ConstantBuffer<BindlessFixtureStorePushConstants> storePushConstants;

[shader("compute")]
[numthreads(1, 1, 1)]
void bindlessStore(uint3 id : SV_DispatchThreadID)
{
    storageTextures[storePushConstants->TextureIndex].Store(id.xy, storePushConstants->Colour);
}
