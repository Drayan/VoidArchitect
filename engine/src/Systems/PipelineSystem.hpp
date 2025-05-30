//
// Created by Michael Desmedt on 27/05/2025.
//
#pragma once
#include "Resources/Pipeline.hpp"
#include "Resources/Shader.hpp"

namespace VoidArchitect
{
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

        // TODO VertexFormat -> How vertex data is structured?
        VertexFormat vertexFormat = VertexFormat::Position;
        std::vector<VertexAttribute> vertexAttributes;

        // TODO InputLayout; -> Which data bindings are used?
        PipelineInputLayout inputLayout;

        // TODO RenderState -> Allow configuration options like culling, depth testing, etc.
        // TODO RenderPass
    };

    class PipelineSystem
    {
    public:
        PipelineSystem();
        ~PipelineSystem() = default;

        Resources::PipelinePtr CreatePipeline(PipelineConfig& config);

        Resources::PipelinePtr& GetDefaultPipeline() { return m_DefaultPipeline; };

    private:
        void GenerateDefaultPipelines();
        uint32_t GetFreePipelineHandle();
        void ReleasePipeline(const Resources::IPipeline* pipeline);

        struct PipelineDeleter
        {
            PipelineSystem* system;
            void operator()(const Resources::IPipeline* pipeline) const;
        };

        std::unordered_map<UUID, std::weak_ptr<Resources::IPipeline>> m_PipelineCache;

        Resources::PipelinePtr m_DefaultPipeline;
    };

    inline std::unique_ptr<PipelineSystem> g_PipelineSystem;

} // namespace VoidArchitect
