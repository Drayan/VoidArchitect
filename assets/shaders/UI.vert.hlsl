/*
---
shader:
    stage: "vertex"
---
*/
#include "VoidArchitect.hlsl"

ConstantBuffer<GlobalUBO> g_ubo : register(b0, space0);
[[vk::push_constant]] Constants g_constants;

DataTransfer main(VSInput input)
{
    DataTransfer output;
    output.Position = mul(g_ubo.UIProjection, mul(g_constants.Model, float4(input.Position.xy, 0.0f, 1.0f)));
    output.UV0 = -input.UV0;

    return output;
}