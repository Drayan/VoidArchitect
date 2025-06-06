/*
---
shader:
    stage: "vertex"
---
*/
#include "VoidArchitect.hlsl"

ConstantBuffer<GlobalUBO> g_ubo : register(b0, space0);
[[vk::push_constant]] Constants g_constants;

PSInput main(VSInput input)
{
    PSInput output;
    output.PixelPosition = mul(g_constants.Model, float4(input.Position, 1.0));
    output.Position = mul(g_ubo.Projection, mul(g_ubo.View, mul(g_constants.Model, float4(input.Position, 1.0))));
    output.Normal = mul(g_constants.Model, float4(input.Normal, 0.0)).xyz;
    output.UV0 = input.UV0;
    return output;
}