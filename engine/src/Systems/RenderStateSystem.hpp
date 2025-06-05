//
// Created by Michael Desmedt on 27/05/2025.
//
#pragma once
#include "RenderPassSystem.hpp"
#include "Resources/RenderPass.hpp"
#include "Resources/RenderState.hpp"
#include "Resources/Shader.hpp"

namespace VoidArchitect
{
    namespace Renderer
    {
        enum class TextureFormat;
        enum class RenderPassType;
    } // namespace Renderer

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

    enum class VertexAttributeType
    {
        Float,
        Vec2,
        Vec3,
        Vec4,
        Mat4,
    };

    struct VertexAttribute
    {
        VertexAttributeType type;
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

    struct ResourceBinding
    {
        ResourceBindingType type;
        uint32_t binding;
        Resources::ShaderStage stage;
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

    struct RenderStateConfig
    {
        std::string name;
        VAArray<Resources::ShaderPtr> shaders;

        VAArray<Renderer::RenderPassType> compatiblePassTypes;
        VAArray<std::string> compatiblePassNames;

        // TODO VertexFormat -> How vertex data is structured?
        VertexFormat vertexFormat = VertexFormat::Position;
        VAArray<VertexAttribute> vertexAttributes;

        // TODO InputLayout; -> Which data bindings are used?
        RenderStateInputLayout inputLayout;

        // TODO RenderState -> Allow configuration options like culling, depth testing, etc.
    };

    struct RenderStateSignature
    {
        VAArray<Renderer::TextureFormat> colorFormats;
        std::optional<Renderer::TextureFormat> depthFormat;

        bool operator==(const RenderStateSignature& other) const;
        [[nodiscard]] size_t GetHash() const;
    };

    struct RenderStateCacheKey
    {
        std::string templateName;
        RenderStateSignature signature;

        bool operator==(const RenderStateCacheKey& other) const;
        [[nodiscard]] size_t GetHash() const;
    };
} // namespace VoidArchitect

namespace std
{
    template <>
    struct hash<VoidArchitect::RenderStateSignature>
    {
        size_t operator()(const VoidArchitect::RenderStateSignature& signature) const noexcept
        {
            return signature.GetHash();
        }
    };

    template <>
    struct hash<VoidArchitect::RenderStateCacheKey>
    {
        size_t operator()(const VoidArchitect::RenderStateCacheKey& key) const noexcept
        {
            return key.GetHash();
        }
    };
} // namespace std

namespace VoidArchitect
{
    class RenderStateSystem
    {
    public:
        RenderStateSystem();
        ~RenderStateSystem() = default;

        void RegisterRenderStateTemplate(const std::string& name, const RenderStateConfig& config);
        [[nodiscard]] bool HasRenderStateTemplate(const std::string& name) const;
        [[nodiscard]] const RenderStateConfig& GetRenderStateTemplate(
            const std::string& name) const;

        Resources::RenderStatePtr CreateRenderState(
            const std::string& templateName,
            const RenderPassConfig& passConfig,
            const Resources::RenderPassPtr& renderPass);
        Resources::RenderStatePtr GetCachedRenderState(
            const std::string& templateName,
            const RenderStateSignature& signature);

        void ClearCache();

        [[nodiscard]] bool IsRenderStateCompatibleWithPass(
            const std::string& renderStateName,
            Renderer::RenderPassType passType) const;
        [[nodiscard]] VAArray<std::string> GetCompatibleRenderStatesForPass(
            Renderer::RenderPassType passType) const;

        RenderStateSignature CreateSignatureFromPass(const RenderPassConfig& passConfig);

    private:
        void GenerateDefaultRenderStates();

        VAHashMap<std::string, RenderStateConfig> m_RenderStateTemplates;
        VAHashMap<RenderStateCacheKey, Resources::RenderStatePtr> m_RenderStateCache;

        Resources::RenderStatePtr m_DefaultState;
    };

    inline std::unique_ptr<RenderStateSystem> g_RenderStateSystem;
} // namespace VoidArchitect
