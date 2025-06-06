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
Texture2D<float4> l_texture : register(t1, space1);

SamplerState m_specularMap : register(s2, space1);
Texture2D<float4> m_specularMapTexture : register(t2, space1);

float4 main(PSInput input) : SV_Target
{
    float lightIntensity = max(dot(input.Normal, -g_ubo.LightDirection.xyz), 0.0f);

    float3 viewDir = normalize(g_ubo.ViewPosition - input.PixelPosition).xyz;
    float3 halfDir = normalize(viewDir - g_ubo.LightDirection.xyz);
    float specularIntensity = pow(max(dot(input.Normal, halfDir), 0.0f), 64.0f);

    float4 diffuseTexture = l_texture.Sample(l_sampler, input.UV0);

    float4 diffuseColor = float4(g_ubo.LightColor.rgb * lightIntensity, diffuseTexture.a);
    float4 ambientColor = float4(float3(0.1, 0.1, 0.1) * l_ubo.DiffuseColor.rgb, diffuseTexture.a); // Ambient light color
    float4 specularColor = float4(g_ubo.LightColor.rgb * specularIntensity, diffuseTexture.a);

    diffuseColor *= diffuseTexture;
    ambientColor *= diffuseTexture;
    specularColor *= float4(m_specularMapTexture.Sample(m_specularMap, input.UV0).rrr, diffuseTexture.a);

    return (diffuseColor + ambientColor + specularColor);
}