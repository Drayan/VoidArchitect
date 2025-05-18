struct PSInput
{
    float3 Color : COLOR0;
};

struct PSOutput
{
    float4 Color;
    float Depth;
};

PSOutput main(PSInput input) : SV_Target
{
    PSOutput output;
    output.Color = float4(input.Color, 1);
    output.Depth = 1;

    return output;
}