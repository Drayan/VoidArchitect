//
// Created by Michael Desmedt on 08/06/2025.
//
#pragma once
#include "../../../common/src/Core.hpp"
#include "Core/Math/Vec4.hpp"
#include "Resources/RenderTarget.hpp"

namespace VoidArchitect::Resources
{
    enum class ShaderStage;
}

namespace VoidArchitect::Renderer
{
    class IPassRenderer;

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

        bool operator<(const ResourceBinding& other) const
        {
            return binding < other.binding;
        }
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

    enum class MaterialClass
    {
        Standard,
        UI
    };

    struct FrameData
    {
        float deltaTime;
    };

    struct RenderPassConfig
    {
        std::string name;
        Renderer::RenderPassType type = Renderer::RenderPassType::Unknown;

        struct AttachmentConfig
        {
            std::string name;

            Renderer::TextureFormat format;
            Renderer::LoadOp loadOp = Renderer::LoadOp::Clear;
            Renderer::StoreOp storeOp = Renderer::StoreOp::Store;

            // Clear values (used if LoadOp is Clear)
            Math::Vec4 clearColor = Math::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
            float clearDepth = 1.0f;
            uint32_t clearStencil = 0;

            bool operator==(const AttachmentConfig&) const;
        };

        VAArray<AttachmentConfig> attachments;

        bool operator==(const RenderPassConfig&) const;
    };

    //==============================================================================================
    // Render Graph Execution Plan Structures
    //==============================================================================================

    static constexpr auto WELL_KNOWN_RT_VIEWPORT_COLOR = "ViewportColorOutput";
    static constexpr auto WELL_KNOWN_RT_VIEWPORT_DEPTH = "ViewportDepthOutput";

    struct RenderPassStep
    {
        // --- Identity ---
        std::string name; // For debugging (e.g., "ForwardOpaque", "UI")
        // The PassRenderer that will record the draw commands.
        IPassRenderer* passRenderer = nullptr;

        // --- RHI Configuration ---
        // The configuration for the render pass itself (attachments, formats).
        RenderPassConfig passConfig;
        PassPosition passPosition; // The position in the frame.

        // --- Resource Binding ---
        // The concrete handles to the render targets to be bound.
        VAArray<Resources::RenderTargetHandle> renderTargets;

        // TODO: We will add barrier information here later.
    };

    struct RenderGraphExecutionPlan
    {
        VAArray<RenderPassStep> steps;

        // Allows for each iteration: for(const auto& step: executionPlan)
        auto begin() const { return steps.begin(); }
        auto end() const { return steps.end(); }
        bool empty() const { return steps.empty(); }
        size_t size() const { return steps.size(); }
    };
} // namespace Renderer
