//static float2 positions[3] = float2[](
//    float2(0.5, -0.5),
//    float2(0.0, 0.5),
//    float2(-0.5, -0.5)
//);

//static float3 colors[3] = float3[](
//    float3(1.0, 0.0, 0.0),
//    float3(0.0, 1.0, 0.0),
//    float3(0.0, 0.0, 1.0)
//);

struct Vertex
{
    float3 position;
    float padding;
    float4 color;
};

struct MeshDrawPushConstants
{
    Vertex* vertices;
};

[[vk::push_constant]] ConstantBuffer<MeshDrawPushConstants> meshDrawConstants;

struct VertexOutput {
    float4 color;
    float4 sv_position : SV_Position;
};

[shader("vertex")]
VertexOutput VertMain(uint vid : SV_VertexID) {
    VertexOutput output;
    Vertex vertex = meshDrawConstants->vertices[vid];
    output.sv_position = float4(vertex.position, 1.0);
    output.color = vertex.color;
    return output;
}

[shader("fragment")]
float4 FragMain(VertexOutput inVert) : SV_Target
{
    return inVert.color;
}