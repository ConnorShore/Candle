// Fixture for PipelineStoreTests: a vertex and fragment pair with no vertex input, as the engine's pipelines have for now,
// and a compute entry point beside them, as Triangle.slang has.

struct VSOutput
{
    float4 position : SV_Position;
};

[shader("vertex")]
VSOutput pipelineVertex(uint vertexId : SV_VertexID)
{
    VSOutput output;
    output.position = float4(float(vertexId & 1), float(vertexId >> 1), 0.0, 1.0);
    return output;
}

[shader("fragment")]
float4 pipelineFragment(VSOutput input) : SV_Target
{
    return float4(1.0, 0.0, 1.0, 1.0);
}

// Never dispatched, so it touches nothing. The tests assert its group size, uneven on every axis so a swapped one shows.
[shader("compute")]
[numthreads(8, 4, 2)]
void pipelineCompute(uint3 id : SV_DispatchThreadID)
{
}
