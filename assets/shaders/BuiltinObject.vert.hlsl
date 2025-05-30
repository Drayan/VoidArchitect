/*
---
shader:
    stage: "vertex"
---
*/
struct VS_INPUT
{
    [[vk::location(0)]]
    float3 Position : POSITION;
    [[vk::location(1)]]
    float2 UV0 : TEXCOORD0;
};

struct VS_OUTPUT
{
    [[vk::location(0)]]
    float4 Position : SV_POSITION;
    [[vk::location(1)]]
    float2 UV0 : TEXCOORD0;
};

struct UBO
{
    float4x4 Projection;
    float4x4 View;
};
ConstantBuffer<UBO> g_ubo : register(b0, space0);

struct Constants
{
    float4x4 Model;
};
[[vk::push_constant]] Constants g_constants;

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    output.Position = mul(g_ubo.Projection, mul(g_ubo.View, mul(g_constants.Model, float4(input.Position, 1.0))));
    output.UV0 = input.UV0;
    return output;
}