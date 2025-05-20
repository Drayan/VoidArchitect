struct VS_INPUT
{
    [[vk::location(0)]]
    float3 Position : POSITION;
};

struct UBO
{
    float4x4 Projection;
    float4x4 View;
};
ConstantBuffer<UBO> g_ubo : register(b0, space0);

float4 main(VS_INPUT input) : SV_POSITION
{
    return mul(g_ubo.Projection, mul(g_ubo.View, float4(input.Position, 1.0)));
}