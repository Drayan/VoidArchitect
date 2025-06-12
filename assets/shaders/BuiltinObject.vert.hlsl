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

    float3x3 modelMat = (float3x3)g_constants.Model;
    output.Normal = normalize(mul(modelMat, input.Normal));
    output.Tangent = float4(normalize(mul(modelMat, input.Tangent.xyz)), input.Tangent.w);

    output.UV0 = input.UV0;
    return output;
}