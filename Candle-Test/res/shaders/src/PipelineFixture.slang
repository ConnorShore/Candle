// Fixture for PipelineTests: a vertex and fragment pair with no vertex input, as the engine's pipelines have for now.
// No compute entry point, because Pipeline turns every entry point in the module into a stage of one graphics pipeline.

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
