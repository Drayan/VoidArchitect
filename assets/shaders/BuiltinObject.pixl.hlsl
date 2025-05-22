struct PSInput
{
    [[vk::location(1)]]
    float2 UV0 : TEXCOORD0;
};

struct UBO
{
    float4 DiffuseColor;
};
ConstantBuffer<UBO> l_ubo : register(b0, space1);

SamplerState l_sampler : register(s1, space1);
Texture2D l_texture : register(t1, space1);

float4 main(PSInput input) : SV_Target
{
    return l_texture.Sample(l_sampler, input.UV0) * l_ubo.DiffuseColor;
}