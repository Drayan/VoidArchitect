struct PSInput
{
    [[vk::location(0)]] float4 Position : SV_POSITION;
};

struct PSOutput
{
    float4 Color;
};

PSOutput main(PSInput input) : SV_Target
{
    PSOutput output;
    output.Color = float4(1, 0, 0, 1);

    return output;
}