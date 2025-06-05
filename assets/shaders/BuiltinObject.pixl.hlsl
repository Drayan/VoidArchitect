/*
---
shader:
    stage: "pixel"
---
*/
#include "VoidArchitect.hlsl"

ConstantBuffer<GlobalUBO> g_ubo : register(b0, space0);
ConstantBuffer<MaterialUBO> l_ubo : register(b0, space1);

SamplerState l_sampler : register(s1, space1);
Texture2D l_texture : register(t1, space1);

float4 main(PSInput input) : SV_Target
{
    float lightIntensity = max(dot(input.Normal, -g_ubo.LightDirection.xyz), 0.0f);

    float4 diffuseTexture = l_texture.Sample(l_sampler, input.UV0);

    float4 diffuseColor = float4(g_ubo.LightColor.rgb * lightIntensity, diffuseTexture.a);
    float4 ambientColor = float4(float3(0.1, 0.1, 0.1) * diffuseTexture.rgb, diffuseTexture.a); // Ambient light color
    return (diffuseColor * diffuseTexture * l_ubo.DiffuseColor) + ambientColor;
}