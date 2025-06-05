
struct VSInput
{
    [[vk::location(0)]]
    float3 Position : POSITION;
    [[vk::location(1)]]
    float3 Normal : NORMAL;
    [[vk::location(2)]]
    float2 UV0 : TEXCOORD0;
};

struct DataTransfer
{
    [[vk::location(0)]]
    float4 Position : SV_POSITION;
    [[vk::location(1)]]
    float3 Normal : NORMAL;
    [[vk::location(2)]]
    float2 UV0 : TEXCOORD0;
};

struct PSInput
{
    [[vk::location(1)]]
    float3 Normal : NORMAL;
    [[vk::location(2)]]
    float2 UV0 : TEXCOORD0;
};

struct GlobalUBO
{
    float4x4 Projection;
    float4x4 View;
    float4x4 UIProjection;
    float4 LightDirection;
    float4 LightColor;
};
struct MaterialUBO
{
    float4 DiffuseColor;
};

struct Constants
{
    float4x4 Model;
};