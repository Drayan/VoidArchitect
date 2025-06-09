//
// Created by Michael Desmedt on 08/06/2025.
//
#pragma once
#include "Core/Core.hpp"

namespace VoidArchitect::Resources
{
    enum class ShaderStage;
}

namespace VoidArchitect::Renderer
{
    enum class LoadOp
    {
        Load,
        Clear,
        DontCare
    };

    enum class StoreOp
    {
        Store,
        DontCare
    };

    // TODO : Add support for more formats
    enum class TextureFormat
    {
        RGBA8_UNORM,
        BGRA8_UNORM,
        RGBA8_SRGB,
        BGRA8_SRGB,

        D32_SFLOAT,
        D24_UNORM_S8_UINT,

        SWAPCHAIN_FORMAT,
        SWAPCHAIN_DEPTH
    };

    enum class RenderPassType
    {
        ForwardOpaque,
        ForwardTransparent,
        Shadow,
        DepthPrepass,
        PostProcess,
        UI,
        Unknown
    };

    enum class PassPosition
    {
        First, // UNDEFINED -> COLOR_ATTACHMENT
        Middle, // COLOR_ATTACHMENT -> COLOR_ATTACHMENT
        Last, // COLOR_ATTACHMENT -> PRESENT
        Standalone // UNDEFINED -> PRESENT
    };

    enum class VertexFormat
    {
        Position,
        PositionColor,
        PositionUV,
        PositionNormal,
        PositionNormalUV,
        PositionNormalUVTangent,
        Custom
    };

    enum class AttributeFormat
    {
        Float32,
    };

    enum class AttributeType
    {
        Float,
        Vec2,
        Vec3,
        Vec4,
        Mat4,
    };

    struct VertexAttribute
    {
        AttributeType type;
        AttributeFormat format;
    };

    enum class ResourceBindingType
    {
        ConstantBuffer,
        Texture1D,
        Texture2D,
        Texture3D,
        TextureCube,
        Sampler,
        StorageBuffer,
        StorageTexture
    };

    ResourceBindingType ResourceBindingTypeFromString(const std::string& str);

    struct BufferBinding
    {
        uint32_t binding;
        AttributeType type;
        AttributeFormat format;
    };

    struct ResourceBinding
    {
        ResourceBindingType type;
        uint32_t binding;
        VoidArchitect::Resources::ShaderStage stage;

        std::optional<std::vector<BufferBinding>> bufferBindings;
    };

    struct SpaceLayout
    {
        uint32_t space;
        VAArray<ResourceBinding> bindings;
    };

    struct RenderStateInputLayout
    {
        VAArray<SpaceLayout> spaces;
    };

    enum class RenderTargetUsage : uint32_t
    {
        None = 0,
        ColorAttachment = BIT(0),
        DepthStencilAttachment = BIT(1),
        RenderTexture = BIT(2),
        Storage = BIT(3)
    };

    struct RenderTargetConfig
    {
        std::string name;
        TextureFormat format;
        RenderTargetUsage usage = RenderTargetUsage::ColorAttachment;

        enum class SizingPolicy
        {
            Absolute,
            RelativeToViewport,
        };

        SizingPolicy sizingPolicy = SizingPolicy::RelativeToViewport;
        uint32_t width = 0;
        uint32_t height = 0;
    };
} // namespace Renderer
