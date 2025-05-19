struct PS_INPUT
{
    [[vk::location(0)]] float3 Position : POSITION;
};

float4 main(PS_INPUT input) : SV_Target
{
    return float4(input.Position + 0.5, 1.0);
}