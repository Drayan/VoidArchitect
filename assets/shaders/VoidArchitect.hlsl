
struct VSInput
{
    [[vk::location(0)]]
    float3 Position : POSITION;
    [[vk::location(1)]]
    float3 Normal : NORMAL;
    [[vk::location(2)]]
    float2 UV0 : TEXCOORD0;
    [[vk::location(3)]]
    float4 Tangent : TANGENT;
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
    float4 Tangent : TANGENT;
    [[vk::location(4)]]
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
    uint DebugMode;
};
struct MaterialUBO
{
    float4 DiffuseColor;
};

struct Constants
{
    float4x4 Model;
};

struct PointLight
{
    float3 Position;
    float4 Color;
    float Constant;
    float Linear;
    float Quadratic;
};

// TODO: Implement a proper lighting system that will provide these
static PointLight g_PointLights[3] = {
    {float3(0.0f, 1.5f, 0.0f), float4(0.0f, 1.0f, 0.0f, 1.0f), 1.0f, 0.35f, 0.032f},
    {float3(0.0f, 0.0f, -1.2f), float4(1.0f, 0.0f, 0.0f, 1.0f), 1.0f, 0.35f, 0.032f},
    {float3(-1.2f, 0.0f, 0.0f), float4(0.0f, 0.0f, 1.0f, 1.0f), 1.0f, 0.35f, 0.032f}};