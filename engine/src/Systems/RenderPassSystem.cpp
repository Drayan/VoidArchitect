//
// Created by Michael Desmedt on 04/06/2025.
//

#include "RenderPassSystem.hpp"

#include "Core/Logger.hpp"
#include "Platform/RHI/IRenderingHardware.hpp"
#include "Renderer/RenderCommand.hpp"
#include "Renderer/RenderGraph.hpp"
#include "Renderer/PassRenderers.hpp"

namespace VoidArchitect
{
    RenderPassSystem::RenderPassSystem()
    {
        GenerateDefaultRenderPasses();
        RegisterDefaultRenderers();
    }

    UUID RenderPassSystem::RegisterRenderPassTemplate(
        const std::string& name,
        const RenderPassConfig& config)
    {
        const RenderPassTemplate passTemplate{
            UUID(),
            name,
            config
        };

        m_RenderPassTemplates[passTemplate.UUID] = passTemplate;
        m_TemplatesNameToUUIDMap[passTemplate.Name] = passTemplate.UUID;

        VA_ENGINE_TRACE(
            "[RenderPassSystem] RenderPass template '{}' registered with UUID {}.",
            name,
            static_cast<uint64_t>(passTemplate.UUID));

        return passTemplate.UUID;
    }

    bool RenderPassSystem::HasRenderPassTemplate(const UUID& uuid) const
    {
        return m_RenderPassTemplates.contains(uuid);
    }

    bool RenderPassSystem::HasRenderPassTemplate(const std::string& name) const
    {
        if (m_TemplatesNameToUUIDMap.contains(name))
        {
            const auto uuid = m_TemplatesNameToUUIDMap.at(name);
            return m_RenderPassTemplates.contains(uuid);
        }

        return false;
    }

    const RenderPassConfig& RenderPassSystem::GetRenderPassTemplate(const UUID& uuid) const
    {
        const auto it = m_RenderPassTemplates.find(uuid);
        if (it == m_RenderPassTemplates.end())
        {
            VA_ENGINE_ERROR(
                "[RenderPassSystem] RenderPass template '{}' not found.",
                static_cast<uint64_t>(uuid));
            static const RenderPassConfig emptyConfig{};
            return emptyConfig;
        }

        return it->second.Config;
    }

    UUID RenderPassSystem::GetRenderPassTemplateUUID(const std::string& name) const
    {
        if (!m_TemplatesNameToUUIDMap.contains(name))
        {
            VA_ENGINE_ERROR("[RenderPassSystem] RenderPass template '{}' not found.", name);
            return InvalidUUID;
        }

        return m_TemplatesNameToUUIDMap.at(name);
    }

    void RenderPassSystem::RegisterPassRenderer(Renderer::PassRendererPtr& renderer)
    {
        if (!renderer)
        {
            VA_ENGINE_WARN("[RenderPassSystem] Attempted to register a null pass renderer.");
            return;
        }

        // Find comaptible pass types
        for (uint32_t i = 0; i < static_cast<uint32_t>(Renderer::RenderPassType::Unknown); i++)
        {
            if (const auto passType = static_cast<Renderer::RenderPassType>(i); renderer->
                IsCompatibleWith(passType))
            {
                if (m_PassRenderers.contains(passType))
                {
                    VA_ENGINE_WARN(
                        "[RenderPassSystem] Pass renderer '{}' is already registered for pass type '{}'.",
                        renderer->GetName(),
                        Renderer::RenderPassTypeToString(passType));
                }

                VA_ENGINE_TRACE(
                    "[RenderPassSystem] Registering pass renderer '{}' for pass type '{}'.",
                    renderer->GetName(),
                    Renderer::RenderPassTypeToString(passType));

                m_AvailableRenderersNames.push_back(renderer->GetName());
                m_PassRenderers[passType] = std::move(renderer);
                break;
            }
        }
    }

    Renderer::IPassRenderer* RenderPassSystem::GetPassRenderer(
        const Renderer::RenderPassType type) const
    {
        if (const auto it = m_PassRenderers.find(type); it != m_PassRenderers.end())
        {
            return it->second.get();
        }

        VA_ENGINE_WARN(
            "[RenderPassSystem] No pass renderer found for pass type '{}'.",
            Renderer::RenderPassTypeToString(type));
        return nullptr;
    }

    Resources::RenderPassPtr RenderPassSystem::CreateRenderPass(const UUID& templateUUID)
    {
        // Check if the render pass template exists
        if (!HasRenderPassTemplate(templateUUID))
        {
            VA_ENGINE_ERROR(
                "[RenderPassSystem] RenderPass template '{}' not found.",
                static_cast<uint64_t>(templateUUID));
            return nullptr;
        }

        // Create the signature
        const auto& passTemplate = GetRenderPassTemplate(templateUUID);
        const auto signature = CreateSignature(passTemplate);

        // Check if the render state isn't already in the cache
        const RenderPassCacheKey cacheKey{templateUUID, signature};
        if (auto cached = GetCachedRenderPass(templateUUID, signature))
        {
            VA_ENGINE_DEBUG("[RenderPassSystem] Using cached render pass '{}'.", passTemplate.Name);
            return cached;
        }

        // Create a new RenderPass resource
        const auto rawPass = Renderer::RenderCommand::GetRHIRef().CreateRenderPass(passTemplate);
        if (!rawPass)
        {
            VA_ENGINE_WARN(
                "[RenderPassSystem] Failed to create render pass '{}'.",
                passTemplate.Name);
            return nullptr;
        }

        // Store the render pass in the cache
        auto renderPassPtr = Resources::RenderPassPtr(rawPass);

        m_RenderPassCache[cacheKey] = renderPassPtr;

        VA_ENGINE_TRACE(
            "[RenderPassSystem] Render pass '{}' (Type: {}) created.",
            passTemplate.Name,
            Renderer::RenderPassTypeToString(passTemplate.Type));

        return renderPassPtr;
    }

    Resources::RenderPassPtr RenderPassSystem::GetCachedRenderPass(
        const UUID& templateUUID,
        const RenderPassSignature& signature)
    {
        const RenderPassCacheKey cacheKey(templateUUID, signature);
        if (const auto it = m_RenderPassCache.find(cacheKey); it != m_RenderPassCache.end())
        {
            if (auto& cachedPass = it->second)
            {
                return cachedPass;
            }

            VA_ENGINE_WARN(
                "[RenderPassSystem] Cached render pass '{}' is expired.",
                static_cast<uint64_t>(templateUUID));
            m_RenderPassCache.erase(it);
        }

        return nullptr;
    }

    RenderPassSignature RenderPassSystem::CreateSignature(const RenderPassConfig& config)
    {
        RenderPassSignature signature;

        for (const auto& attachment : config.Attachments)
        {
            signature.AttachmentFormats.push_back(attachment.Format);
        }

        return signature;
    }

    void RenderPassSystem::ClearCache()
    {
        m_RenderPassCache.clear();
        VA_ENGINE_DEBUG("[RenderPassSystem] Cache cleared.");
    }

    void RenderPassSystem::GenerateDefaultRenderPasses()
    {
        // Forward Opaque Pass Template
        RenderPassConfig forwardPassConfig{
            .Name = "ForwardOpaque",
            .Type = Renderer::RenderPassType::ForwardOpaque,
            // TODO: Should also take an UUID from RenderStateSystem
            .CompatibleStates = {"Default"},
            .Attachments = {
                RenderPassConfig::AttachmentConfig{
                    .Name = "color",
                    .Format = Renderer::TextureFormat::SWAPCHAIN_FORMAT,
                    .LoadOp = Renderer::LoadOp::Clear,
                    .StoreOp = Renderer::StoreOp::Store,
                    .ClearColor = Math::Vec4(0.2f, 0.2f, 0.2f, 1.0f),
                },
                RenderPassConfig::AttachmentConfig{
                    .Name = "depth",
                    .Format = Renderer::TextureFormat::SWAPCHAIN_DEPTH,
                    .LoadOp = Renderer::LoadOp::Clear,
                    .StoreOp = Renderer::StoreOp::DontCare,
                    .ClearDepth = 1.0f,
                    .ClearStencil = 0,
                }
            }
        };

        RegisterRenderPassTemplate("ForwardOpaque", forwardPassConfig);

        // TODO: Add other passes template

        VA_ENGINE_INFO("[RenderPassSystem] Default RenderPass templates registered.");
    }

    void RenderPassSystem::RegisterDefaultRenderers()
    {
        // Forward Opaque Pass Renderer
        auto forwardOpaque = Renderer::PassRendererPtr(
            std::make_shared<Renderer::ForwardOpaquePassRenderer>());
        RegisterPassRenderer(forwardOpaque);
    }

    size_t RenderPassSignature::GetHash() const
    {
        size_t hash = 0;
        for (const auto& format : AttachmentFormats)
        {
            hash ^= std::hash<int>{}(static_cast<int>(format)) + 0x9e3779b9
                + (hash << 6) + (hash >> 2);
        }
        return hash;
    }

    size_t RenderPassCacheKey::GetHash() const
    {
        return std::hash<UUID>{}(templateUUID) ^ Signature.GetHash();
    }

    bool RenderPassCacheKey::operator==(const RenderPassCacheKey& rhs) const
    {
        return templateUUID == rhs.templateUUID && Signature == rhs.Signature;
    }

    bool RenderPassSignature::operator==(const RenderPassSignature& rhs) const
    {
        return AttachmentFormats == rhs.AttachmentFormats;
    }
}
