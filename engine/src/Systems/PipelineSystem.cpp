//
// Created by Michael Desmedt on 27/05/2025.
//
#include "PipelineSystem.hpp"

#include "Core/Logger.hpp"
#include "Platform/RHI/IRenderingHardware.hpp"
#include "Renderer/RenderCommand.hpp"
#include "ShaderSystem.hpp"
#include "Renderer/RenderGraph.hpp"

namespace VoidArchitect
{
    size_t PipelineSignature::GetHash() const
    {
        size_t hash = 0;
        for (const auto& format : colorFormats)
        {
            hash ^= std::hash<int>{}(
                static_cast<int>(format) + 0x9e3779b9 + (hash << 6) + (hash >> 2));
        }

        if (depthFormat.has_value())
        {
            hash ^= std::hash<int>{}(
                static_cast<int>(depthFormat.value()) + 0x9e3779b9 + (hash << 6) + (hash >> 2));
        }

        return hash;
    }

    size_t PipelineCacheKey::GetHash() const
    {
        return std::hash<std::string>{}(templateName) ^ signature.GetHash();
    }

    bool PipelineCacheKey::operator==(const PipelineCacheKey& other) const
    {
        return templateName == other.templateName && signature == other.signature;
    }

    bool PipelineSignature::operator==(const PipelineSignature& other) const
    {
        return colorFormats == other.colorFormats && depthFormat == other.depthFormat;
    }

    PipelineSystem::PipelineSystem()
    {
        GenerateDefaultPipelines();
    }

    void PipelineSystem::RegisterPipelineTemplate(
        const std::string& name,
        const PipelineConfig& config)
    {
        m_PipelineTemplates[name] = config;

        VA_ENGINE_TRACE(
            "[PipelineSystem] Pipeline template '{}' registered with compatibility",
            name);
        for (const auto& passType : config.compatiblePassTypes)
        {
            VA_ENGINE_TRACE(
                "[PipelineSystem] - Pass Type: {}",
                Renderer::RenderPassTypeToString(passType));
        }
        for (const auto& passName : config.compatiblePassNames)
        {
            VA_ENGINE_TRACE("[PipelineSystem] - Pass Name: {}", passName);
        }
    }

    bool PipelineSystem::HasPipelineTemplate(const std::string& name) const
    {
        return m_PipelineTemplates.contains(name);
    }

    const PipelineConfig& PipelineSystem::GetPipelineTemplate(const std::string& name) const
    {
        auto it = m_PipelineTemplates.find(name);
        if (it == m_PipelineTemplates.end())
        {
            VA_ENGINE_ERROR("[PipelineSystem] Pipeline template '{}' not found.", name);
            static const PipelineConfig emptyConfig{};
            return emptyConfig;
        }

        return it->second;
    }

    Resources::PipelinePtr PipelineSystem::CreatePipelineForPass(
        const std::string& templateName,
        const Renderer::RenderPassConfig& passConfig,
        const Resources::RenderPassPtr& renderPass)
    {
        // Check if the pipeline template exists
        if (!HasPipelineTemplate(templateName))
        {
            VA_ENGINE_ERROR(
                "[PipelineSystem] Pipeline template '{}' not found for pass '{}'.",
                templateName,
                passConfig.Name);
            return nullptr;
        }

        // Create the signature
        auto signature = CreateSignatureFromPass(passConfig);

        // Check if the pipeline isn't already in the cache
        PipelineCacheKey cacheKey(templateName, signature);
        if (auto cached = GetCachedPipeline(templateName, signature))
        {
            VA_ENGINE_DEBUG(
                "[PipelineSystem] Using cached pipeline '{}' for pass '{}'.",
                templateName,
                passConfig.Name);
            return cached;
        }

        auto config = GetPipelineTemplate(templateName);

        // Add required data to the config
        switch (config.vertexFormat)
        {
            case VertexFormat::Position:
            {
                config.vertexAttributes.push_back(
                    VertexAttribute{VertexAttributeType::Vec3, AttributeFormat::Float32});
            }
            break;
            case VertexFormat::PositionColor:
            {
                config.vertexAttributes.push_back(
                    VertexAttribute{VertexAttributeType::Vec3, AttributeFormat::Float32});
                config.vertexAttributes.push_back(
                    VertexAttribute{VertexAttributeType::Vec4, AttributeFormat::Float32});
            }
            break;
            case VertexFormat::PositionNormal:
            {
                config.vertexAttributes.push_back(
                    VertexAttribute{VertexAttributeType::Vec3, AttributeFormat::Float32});
                config.vertexAttributes.push_back(
                    VertexAttribute{VertexAttributeType::Vec3, AttributeFormat::Float32});
            }
            break;
            case VertexFormat::PositionNormalUV:
            {
                config.vertexAttributes.push_back(
                    VertexAttribute{VertexAttributeType::Vec3, AttributeFormat::Float32});
                config.vertexAttributes.push_back(
                    VertexAttribute{VertexAttributeType::Vec3, AttributeFormat::Float32});
                config.vertexAttributes.push_back(
                    VertexAttribute{VertexAttributeType::Vec2, AttributeFormat::Float32});
            }
            break;
            case VertexFormat::PositionNormalUVTangent:
            {
                config.vertexAttributes.push_back(
                    VertexAttribute{VertexAttributeType::Vec3, AttributeFormat::Float32});
                config.vertexAttributes.push_back(
                    VertexAttribute{VertexAttributeType::Vec3, AttributeFormat::Float32});
                config.vertexAttributes.push_back(
                    VertexAttribute{VertexAttributeType::Vec2, AttributeFormat::Float32});
                config.vertexAttributes.push_back(
                    VertexAttribute{VertexAttributeType::Vec3, AttributeFormat::Float32});
            }
            break;
            case VertexFormat::PositionUV:
            {
                config.vertexAttributes.push_back(
                    VertexAttribute{VertexAttributeType::Vec3, AttributeFormat::Float32});
                config.vertexAttributes.push_back(
                    VertexAttribute{VertexAttributeType::Vec2, AttributeFormat::Float32});
            }
            break;

            // TODO Implement custom vertex format definition (from config file ?)
            case VertexFormat::Custom:
            default:
                VA_ENGINE_WARN(
                    "[PipelineSystem] Unknown vertex format for pipeline '{}'.",
                    config.name);
                break;
        }

        // Create a new pipeline resource
        const auto pipeline = Renderer::RenderCommand::GetRHIRef().CreatePipelineForRenderPass(
            config,
            renderPass.get());

        if (!pipeline)
        {
            VA_ENGINE_WARN(
                "[PipelineSystem] Failed to create pipeline '{}' for pass '{}'.",
                config.name,
                passConfig.Name);
            return nullptr;
        }

        // Store the pipeline in the cache
        auto pipelinePtr = Resources::PipelinePtr(pipeline, PipelineDeleter{this});

        m_CachedPipelines[cacheKey] = pipelinePtr;

        VA_ENGINE_TRACE(
            "[PipelineSystem] Pipeline '{}' created for pass '{}' (Type: {}).",
            config.name,
            passConfig.Name,
            Renderer::RenderPassTypeToString(passConfig.Type));

        return pipelinePtr;
    }

    Resources::PipelinePtr PipelineSystem::GetCachedPipeline(
        const std::string& templateName,
        const PipelineSignature& signature)
    {
        const PipelineCacheKey cacheKey(templateName, signature);
        if (const auto it = m_CachedPipelines.find(cacheKey); it != m_CachedPipelines.end())
        {
            if (auto& cachedPipeline = it->second)
            {
                return cachedPipeline;
            }

            VA_ENGINE_WARN(
                "[PipelineSystem] Cached pipeline '{}' is expired.",
                templateName);
            m_CachedPipelines.erase(it);
        }

        return nullptr;
    }

    void PipelineSystem::ClearCache()
    {
        m_CachedPipelines.clear();
        VA_ENGINE_DEBUG("[PipelineSystem] Cache cleared.");
    }

    bool PipelineSystem::IsPipelineCompatibleWithPass(
        const std::string& pipelineName,
        Renderer::RenderPassType passType) const
    {
        auto it = m_PipelineTemplates.find(pipelineName);
        if (it == m_PipelineTemplates.end())
        {
            VA_ENGINE_WARN(
                "[PipelineSystem] Pipeline '{}' not found in the cache.",
                pipelineName);
            return false;
        }

        const auto& config = it->second;

        return std::ranges::find(config.compatiblePassTypes, passType) != config.compatiblePassTypes
            .end();
    }

    std::vector<std::string> PipelineSystem::GetCompatiblePipelinesForPass(
        Renderer::RenderPassType passType) const
    {
        std::vector<std::string> compatiblePipelines;

        for (const auto& [pipelineName, config] : m_PipelineTemplates)
        {
            if (std::ranges::find(config.compatiblePassTypes, passType) != config.
                compatiblePassTypes
                .end())
            {
                compatiblePipelines.push_back(pipelineName);
            }
        }

        return compatiblePipelines;
    }

    PipelineSignature PipelineSystem::CreateSignatureFromPass(
        const Renderer::RenderPassConfig& passConfig)
    {
        PipelineSignature signature;

        for (const auto& attachment : passConfig.Attachments)
        {
            const bool isDepthAttachment = (attachment.Name == "depth" || attachment.Format ==
                Renderer::TextureFormat::SWAPCHAIN_DEPTH || attachment.Format ==
                Renderer::TextureFormat::D24_UNORM_S8_UINT || attachment.Format ==
                Renderer::TextureFormat::D32_SFLOAT);

            if (isDepthAttachment)
            {
                signature.depthFormat = attachment.Format;
            }
            else
            {
                signature.colorFormats.push_back(attachment.Format);
            }
        }

        return signature;
    }

    void PipelineSystem::GenerateDefaultPipelines()
    {
        auto pipelineConfig = PipelineConfig{};
        pipelineConfig.name = "Default";

        pipelineConfig.compatiblePassTypes = {
            Renderer::RenderPassType::ForwardOpaque,
            Renderer::RenderPassType::DepthPrepass
        };
        pipelineConfig.compatiblePassNames = {"ForwardPass"};

        // Try to load the default shaders into the pipeline.
        auto vertexShader = g_ShaderSystem->LoadShader("BuiltinObject.vert");
        auto pixelShader = g_ShaderSystem->LoadShader("BuiltinObject.pixl");

        pipelineConfig.shaders.emplace_back(vertexShader);
        pipelineConfig.shaders.emplace_back(pixelShader);

        pipelineConfig.vertexFormat = VertexFormat::PositionUV;
        pipelineConfig.inputLayout = PipelineInputLayout{}; // Use default configuration

        RegisterPipelineTemplate("Default", pipelineConfig);

        VA_ENGINE_INFO("[PipelineSystem] Default pipeline template registered.");
    }

    void PipelineSystem::ReleasePipeline(const Resources::IPipeline* pipeline)
    {
        // Remove from the cache
    }

    void PipelineSystem::PipelineDeleter::operator()(const Resources::IPipeline* pipeline) const
    {
        system->ReleasePipeline(pipeline);
        delete pipeline;
    }
} // namespace VoidArchitect
