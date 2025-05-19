struct VS_INPUT
{
    [[vk::location(0)]]
    float3 Position : POSITION;
};

struct VS_OUTPUT
{
    [[vk::location(0)]]
    float3 Position : POSITION;
    float4 OutPosition : SV_POSITION;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    output.Position = input.Position;
    output.OutPosition = float4(input.Position, 1.0);
    return output;
}