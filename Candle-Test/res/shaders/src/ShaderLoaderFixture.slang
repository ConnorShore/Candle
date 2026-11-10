// Fixture for ShaderLoaderTests: one entry point per reflected stage, plus a second compute one, because a
// single .spv may hold several of a stage. The tests assert these names, stages and group sizes; change them together.

struct VSOutput
{
    float4 position : SV_Position;
};

[shader("vertex")]
VSOutput fixtureVertex(uint vertexId : SV_VertexID)
{
    VSOutput output;
    output.position = float4(float(vertexId), 0.0, 0.0, 1.0);
    return output;
}

[shader("fragment")]
float4 fixtureFragment(VSOutput input) : SV_Target
{
    return float4(1.0, 0.0, 1.0, 1.0);
}

RWStructuredBuffer<uint> values;

[shader("compute")]
[numthreads(64, 1, 1)]
void fixtureClear(uint3 id : SV_DispatchThreadID)
{
    values[id.x] = 0;
}

// Differs from fixtureClear on every axis, so each entry point has to report its own
[shader("compute")]
[numthreads(8, 4, 2)]
void fixtureIncrement(uint3 id : SV_DispatchThreadID)
{
    values[id.x] += 1;
}
