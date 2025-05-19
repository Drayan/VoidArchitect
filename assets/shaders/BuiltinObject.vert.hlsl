struct VSInput
{
[[vk::location(0)]] float3 Position : POSITION0;
};

struct VSOutput
{
    float4 Position : SV_POSITION;
};

VSOutput main(VSInput input, uint VertexIndex : SV_VertexID)
{
    VSOutput output = (VSOutput)0;
    output.Position = float4(input.Position, 1.0);

    return output;
}