//
// Created by Michael Desmedt on 27/05/2025.
//
#pragma once
#include <memory>

#include "Resources/Pipeline.hpp"
#include "Resources/RenderPass.hpp"
#include "Resources/Shader.hpp"

namespace VoidArchitect
{
    namespace Renderer
    {
        struct RenderPassConfig;
        enum class TextureFormat;
        enum class RenderPassType;
    }

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
        std::vector<ResourceBinding> bindings;
    };

    struct PipelineInputLayout
    {
        std::vector<SpaceLayout> spaces;
    };

    struct PipelineConfig
    {
        std::string name;
        std::vector<Resources::ShaderPtr> shaders;

        std::vector<Renderer::RenderPassType> compatiblePassTypes;
        std::vector<std::string> compatiblePassNames;

        // TODO VertexFormat -> How vertex data is structured?
        VertexFormat vertexFormat = VertexFormat::Position;
        std::vector<VertexAttribute> vertexAttributes;

        // TODO InputLayout; -> Which data bindings are used?
        PipelineInputLayout inputLayout;

        // TODO RenderState -> Allow configuration options like culling, depth testing, etc.
        // TODO RenderPass
    };

    struct PipelineSignature
    {
        std::vector<Renderer::TextureFormat> colorFormats;
        std::optional<Renderer::TextureFormat> depthFormat;

        bool operator==(const PipelineSignature& other) const;
        [[nodiscard]] size_t GetHash() const;
    };

    struct PipelineCacheKey
    {
        std::string templateName;
        PipelineSignature signature;

        bool operator==(const PipelineCacheKey& other) const;
        [[nodiscard]] size_t GetHash() const;
    };
}

namespace std
{
    template <>
    struct hash<VoidArchitect::PipelineSignature>
    {
        size_t operator()(const VoidArchitect::PipelineSignature& signature) const noexcept
        {
            return signature.GetHash();
        }
    };

    template <>
    struct hash<VoidArchitect::PipelineCacheKey>
    {
        size_t operator()(const VoidArchitect::PipelineCacheKey& key) const noexcept
        {
            return key.GetHash();
        }
    };
}

namespace VoidArchitect
{
    class PipelineSystem
    {
    public:
        PipelineSystem();
        ~PipelineSystem() = default;

        void RegisterPipelineTemplate(const std::string& name, const PipelineConfig& config);
        [[nodiscard]] bool HasPipelineTemplate(const std::string& name) const;
        [[nodiscard]] const PipelineConfig& GetPipelineTemplate(const std::string& name) const;

        Resources::PipelinePtr CreatePipelineForPass(
            const std::string& templateName,
            const Renderer::RenderPassConfig& passConfig,
            const Resources::RenderPassPtr& renderPass);
        Resources::PipelinePtr GetCachedPipeline(
            const std::string& templateName,
            const PipelineSignature& signature);

        void ClearCache();

        [[nodiscard]] bool IsPipelineCompatibleWithPass(
            const std::string& pipelineName,
            Renderer::RenderPassType passType) const;
        [[nodiscard]] std::vector<std::string> GetCompatiblePipelinesForPass(
            Renderer::RenderPassType passType) const;

        PipelineSignature CreateSignatureFromPass(const Renderer::RenderPassConfig& passConfig);

    private:
        void GenerateDefaultPipelines();
        uint32_t GetFreePipelineHandle();
        void ReleasePipeline(const Resources::IPipeline* pipeline);

        struct PipelineDeleter
        {
            PipelineSystem* system;
            void operator()(const Resources::IPipeline* pipeline) const;
        };

        std::unordered_map<std::string, PipelineConfig> m_PipelineTemplates;
        std::unordered_map<PipelineCacheKey, Resources::PipelinePtr> m_CachedPipelines;

        Resources::PipelinePtr m_DefaultPipeline;
    };

    inline std::unique_ptr<PipelineSystem> g_PipelineSystem;
} // namespace VoidArchitect
