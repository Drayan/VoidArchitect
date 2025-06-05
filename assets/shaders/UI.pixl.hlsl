/*
---
shader:
    stage: "pixel"
---
*/
struct PS_INPUT
{
    [[vk::location(2)]]
    float2 UV0 : TEXCOORD0;
};

struct UBO
{
    float4 Color;
};
ConstantBuffer<UBO> l_ubo : register(b0, space1);

SamplerState l_sampler : register(s1, space1);
Texture2D l_texture : register(t1, space1);

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 texColor = l_texture.Sample(l_sampler, input.UV0);
    return texColor * l_ubo.Color;
}
