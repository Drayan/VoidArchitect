
struct VSInput
{
    [[vk::location(0)]]
    float3 Position : POSITION;
    [[vk::location(1)]]
    float3 Normal : NORMAL;
    [[vk::location(2)]]
    float2 UV0 : TEXCOORD0;
};

struct PSInput
{
    [[vk::location(0)]]
    float4 Position : SV_POSITION;
    [[vk::location(1)]]
    float3 Normal : NORMAL;
    [[vk::location(2)]]
    float2 UV0 : TEXCOORD0;
    [[vk::location(3)]]
    float4 PixelPosition : POSITION;
};

struct GlobalUBO
{
    float4x4 Projection;
    float4x4 View;
    float4x4 UIProjection;
    float4 LightDirection;
    float4 LightColor;
    float4 ViewPosition;
};
struct MaterialUBO
{
    float4 DiffuseColor;
};

struct Constants
{
    float4x4 Model;
};