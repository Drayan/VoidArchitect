struct VSInput
{
[[vk::location(0)]] float3 Position : POSITION0;
[[vk::location(1)]] float3 Color : COLOR0;
};

struct VSOutput
{
    float4 Position : SV_POSITION;
[[vk::location(0)]] float3 Color : COLOR0;
};

float2 positions[3] = {
    float2(0.0, -0.5),
    float2(0.5, 0.5),
    float2(-0.5, 0.5)
};

VSOutput main(VSInput input, uint VertexIndex : SV_VertexID)
{
    VSOutput output = (VSOutput)0;
    output.Position = float4(positions[VertexIndex], 0.0, 1.0);

    return output;
}