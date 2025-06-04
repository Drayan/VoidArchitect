//
// Created by Michael Desmedt on 27/05/2025.
//
#include "RenderStateSystem.hpp"

#include "RenderPassSystem.hpp"
#include "Core/Logger.hpp"
#include "Platform/RHI/IRenderingHardware.hpp"
#include "Renderer/RenderCommand.hpp"
#include "Renderer/RenderGraph.hpp"
#include "ShaderSystem.hpp"

namespace VoidArchitect
{
    RenderStateSystem::RenderStateSystem() { GenerateDefaultRenderStates(); }

    void RenderStateSystem::RegisterRenderStateTemplate(
        const std::string& name,
        const RenderStateConfig& config)
    {
        m_RenderStateTemplates[name] = config;

        VA_ENGINE_TRACE(
            "[RenderStateSystem] Pipeline template '{}' registered with compatibility",
            name);
        for (const auto& passType : config.compatiblePassTypes)
        {
            VA_ENGINE_TRACE(
                "[RenderStateSystem] - Pass Type: {}",
                Renderer::RenderPassTypeToString(passType));
        }
        for (const auto& passName : config.compatiblePassNames)
        {
            VA_ENGINE_TRACE("[RenderStateSystem] - Pass Name: {}", passName);
        }
    }

    bool RenderStateSystem::HasRenderStateTemplate(const std::string& name) const
    {
        return m_RenderStateTemplates.contains(name);
    }

    const RenderStateConfig& RenderStateSystem::GetRenderStateTemplate(
        const std::string& name) const
    {
        auto it = m_RenderStateTemplates.find(name);
        if (it == m_RenderStateTemplates.end())
        {
            VA_ENGINE_ERROR("[RenderStateSystem] Pipeline template '{}' not found.", name);
            static const RenderStateConfig emptyConfig{};
            return emptyConfig;
        }

        return it->second;
    }

    Resources::RenderStatePtr RenderStateSystem::CreateRenderState(
        const std::string& templateName,
        const RenderPassConfig& passConfig,
        const Resources::RenderPassPtr& renderPass)
    {
        // Check if the render state template exists
        if (!HasRenderStateTemplate(templateName))
        {
            VA_ENGINE_ERROR(
                "[RenderStateSystem] Pipeline template '{}' not found for pass '{}'.",
                templateName,
                passConfig.Name);
            return nullptr;
        }

        // Create the signature
        auto signature = CreateSignatureFromPass(passConfig);

        // Check if the render state isn't already in the cache
        RenderStateCacheKey cacheKey(templateName, signature);
        if (auto cached = GetCachedRenderState(templateName, signature))
        {
            VA_ENGINE_DEBUG(
                "[RenderStateSystem] Using cached render state '{}' for pass '{}'.",
                templateName,
                passConfig.Name);
            return cached;
        }

        auto config = GetRenderStateTemplate(templateName);

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
                    "[RenderStateSystem] Unknown vertex format for render state '{}'.",
                    config.name);
                break;
        }

        // Create a new render state resource
        const auto renderState =
            Renderer::RenderCommand::GetRHIRef().CreatePipeline(config, renderPass.get());

        if (!renderState)
        {
            VA_ENGINE_WARN(
                "[RenderStateSystem] Failed to create render state '{}' for pass '{}'.",
                config.name,
                passConfig.Name);
            return nullptr;
        }

        // Store the render state in the cache
        auto renderStatePtr = Resources::RenderStatePtr(renderState);

        m_RenderStateCache[cacheKey] = renderStatePtr;

        VA_ENGINE_TRACE(
            "[RenderStateSystem] Pipeline '{}' created for pass '{}' (Type: {}).",
            config.name,
            passConfig.Name,
            Renderer::RenderPassTypeToString(passConfig.Type));

        return renderStatePtr;
    }

    Resources::RenderStatePtr RenderStateSystem::GetCachedRenderState(
        const std::string& templateName,
        const RenderStateSignature& signature)
    {
        const RenderStateCacheKey cacheKey(templateName, signature);
        if (const auto it = m_RenderStateCache.find(cacheKey); it != m_RenderStateCache.end())
        {
            if (auto& cachedPipeline = it->second)
            {
                return cachedPipeline;
            }

            VA_ENGINE_WARN(
                "[RenderStateSystem] Cached render state '{}' is expired.",
                templateName);
            m_RenderStateCache.erase(it);
        }

        return nullptr;
    }

    void RenderStateSystem::ClearCache()
    {
        m_RenderStateCache.clear();
        VA_ENGINE_DEBUG("[RenderStateSystem] Cache cleared.");
    }

    bool RenderStateSystem::IsRenderStateCompatibleWithPass(
        const std::string& renderStateName,
        Renderer::RenderPassType passType) const
    {
        auto it = m_RenderStateTemplates.find(renderStateName);
        if (it == m_RenderStateTemplates.end())
        {
            VA_ENGINE_WARN(
                "[RenderStateSystem] Pipeline '{}' not found in the cache.",
                renderStateName);
            return false;
        }

        const auto& config = it->second;

        return std::ranges::find(config.compatiblePassTypes, passType)
            != config.compatiblePassTypes.end();
    }

    std::vector<std::string> RenderStateSystem::GetCompatibleRenderStatesForPass(
        Renderer::RenderPassType passType) const
    {
        std::vector<std::string> compatiblePipelines;

        for (const auto& [renderStateName, config] : m_RenderStateTemplates)
        {
            if (std::ranges::find(config.compatiblePassTypes, passType)
                != config.compatiblePassTypes.end())
            {
                compatiblePipelines.push_back(renderStateName);
            }
        }

        return compatiblePipelines;
    }

    RenderStateSignature RenderStateSystem::CreateSignatureFromPass(
        const RenderPassConfig& passConfig)
    {
        RenderStateSignature signature;

        for (const auto& attachment : passConfig.Attachments)
        {
            const bool isDepthAttachment =
            (attachment.Name == "depth"
                || attachment.Format == Renderer::TextureFormat::SWAPCHAIN_DEPTH
                || attachment.Format == Renderer::TextureFormat::D24_UNORM_S8_UINT
                || attachment.Format == Renderer::TextureFormat::D32_SFLOAT);

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

    void RenderStateSystem::GenerateDefaultRenderStates()
    {
        auto renderStateConfig = RenderStateConfig{};
        renderStateConfig.name = "Default";

        renderStateConfig.compatiblePassTypes = {
            Renderer::RenderPassType::ForwardOpaque,
            Renderer::RenderPassType::DepthPrepass
        };
        renderStateConfig.compatiblePassNames = {"ForwardPass"};

        // Try to load the default shaders into the render state.
        auto vertexShader = g_ShaderSystem->LoadShader("BuiltinObject.vert");
        auto pixelShader = g_ShaderSystem->LoadShader("BuiltinObject.pixl");

        renderStateConfig.shaders.emplace_back(vertexShader);
        renderStateConfig.shaders.emplace_back(pixelShader);

        renderStateConfig.vertexFormat = VertexFormat::PositionUV;
        renderStateConfig.inputLayout = RenderStateInputLayout{}; // Use default configuration

        RegisterRenderStateTemplate("Default", renderStateConfig);

        VA_ENGINE_INFO("[RenderStateSystem] Default render state template registered.");
    }

    size_t RenderStateSignature::GetHash() const
    {
        size_t hash = 0;
        for (const auto& format : colorFormats)
        {
            hash ^=
                std::hash<int>{}(static_cast<int>(format) + 0x9e3779b9 + (hash << 6) + (hash >> 2));
        }

        if (depthFormat.has_value())
        {
            hash ^= std::hash<int>{}(
                static_cast<int>(depthFormat.value()) + 0x9e3779b9 + (hash << 6) + (hash >> 2));
        }

        return hash;
    }

    size_t RenderStateCacheKey::GetHash() const
    {
        return std::hash<std::string>{}(templateName) ^ signature.GetHash();
    }

    bool RenderStateCacheKey::operator==(const RenderStateCacheKey& other) const
    {
        return templateName == other.templateName && signature == other.signature;
    }

    bool RenderStateSignature::operator==(const RenderStateSignature& other) const
    {
        return colorFormats == other.colorFormats && depthFormat == other.depthFormat;
    }
} // namespace VoidArchitect
