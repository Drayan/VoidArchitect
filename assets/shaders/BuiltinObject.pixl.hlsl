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

SamplerState m_normalMap : register(s3, space1);
Texture2D<float4> m_normalMapTexture : register(t3, space1);

float4 calculateDirectionalLight(PSInput input, float4 lightColor, float3 lightDir, float3 normal, float3 viewDir);
float4 calculatePointLight(PointLight light, PSInput input, float3 normal, float3 viewDir);

float4 main(PSInput input) : SV_Target
{
    // Calculate TBN matrix
    float3 normal = normalize(input.Normal);
    float3 tangent = normalize(input.Tangent.xyz);
    tangent = (tangent - dot(tangent, normal) * normal);
    float3 bitangent = cross(normal, input.Tangent.xyz) * input.Tangent.w;
    float3x3 TBN = float3x3(tangent, bitangent, normal);

    // Update the normal to use the normal map
    float3 localNormal = m_normalMapTexture.Sample(m_normalMap, input.UV0).rgb * 2.0f - 1.0f;
    normal = normalize(mul(localNormal, TBN));

    float4 finalColor;
    if (g_ubo.DebugMode == 0 || g_ubo.DebugMode == 1)
    {
        // Do the normal lighting calculation
        float3 viewDir = normalize(g_ubo.ViewPosition - input.PixelPosition).xyz;
        finalColor = calculateDirectionalLight(input, g_ubo.LightColor, g_ubo.LightDirection.xyz, normal, viewDir);

        finalColor += calculatePointLight(g_PointLights[0], input, normal, viewDir);
        finalColor += calculatePointLight(g_PointLights[1], input, normal, viewDir);
        finalColor += calculatePointLight(g_PointLights[2], input, normal, viewDir);

        return finalColor;
    }
    else if (g_ubo.DebugMode == 2)
    {
        return float4(abs(normal), 1.0f); // Debug normal
    }

    return float4(1.0f, 0.0f, 0.0f, 1.0f);
}

float4 calculateDirectionalLight(PSInput input, float4 lightColor, float3 lightDir, float3 normal, float3 viewDir)
{
    float lightIntensity = max(dot(normal, -lightDir), 0.0f);

    float3 halfDir = normalize(viewDir - lightDir);
    float specularIntensity = pow(max(dot(normal, halfDir), 0.0f), 64.0f);

    float4 rawDiff = l_texture.Sample(l_sampler, input.UV0);
    float4 diffuseColor = float4(lightColor.rgb * lightIntensity, rawDiff.a);
    float4 ambientColor = float4(float3(0.1, 0.1, 0.1) * diffuseColor.rgb, rawDiff.a); // Ambient light color
    float4 specularColor = float4(lightColor.rgb * specularIntensity, rawDiff.a);

    if (g_ubo.DebugMode == 0)
    {
        diffuseColor *= rawDiff;
        ambientColor *= rawDiff;
        specularColor *= float4(m_specularMapTexture.Sample(m_specularMap, input.UV0).rgb, rawDiff.a);
    }

    return diffuseColor + ambientColor + specularColor;
}

float4 calculatePointLight(PointLight light, PSInput input, float3 normal, float3 viewDir)
{
    float3 lightDir = normalize(light.Position - input.PixelPosition.xyz);
    float diff = max(dot(normal, lightDir), 0.0f);

    float3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0f), 64.0f);

    float distance = length(light.Position - input.PixelPosition.xyz);
    float attenuation = 1.0f / (light.Constant + light.Linear * distance + light.Quadratic * distance * distance);

    float4 diffuseColor = light.Color * diff;
    float4 ambientColor = float4(0.1, 0.1, 0.1, 1.0); // Ambient light color
    float4 specularColor = light.Color * spec;

    if (g_ubo.DebugMode == 0)
    {
        float4 rawDiff = l_texture.Sample(l_sampler, input.UV0);
        diffuseColor *= rawDiff;
        ambientColor *= rawDiff;
        specularColor *= float4(m_specularMapTexture.Sample(m_specularMap, input.UV0).rgb, rawDiff.a);
    }

    ambientColor *= attenuation;
    specularColor *= attenuation;
    diffuseColor *= attenuation;

    return (diffuseColor + ambientColor + specularColor);
}