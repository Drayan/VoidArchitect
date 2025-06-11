//
// Created by Michael Desmedt on 04/06/2025.
//

#include "RenderPassSystem.hpp"

#include "Core/Logger.hpp"
#include "Platform/RHI/IRenderingHardware.hpp"
#include "Renderer/RenderSystem.hpp"

namespace VoidArchitect
{
    RenderPassSystem::RenderPassSystem()
    {
    }

    RenderPassHandle RenderPassSystem::GetHandleFor(
        const Renderer::RenderPassConfig& config,
        Renderer::PassPosition position)
    {
        // Check if this renderPass is already in the cache.
        const RenderPassCacheKey key{config, position};
        if (m_RenderPassCache.contains(key))
        {
            // This RenderPass at position exists.
            return m_RenderPassCache[key];
        }

        // This is the first time the system is asked a handle for this RenderPass at this position.
        const auto renderPassPtr = CreateRenderPass(config, position);
        if (!renderPassPtr)
        {
            VA_ENGINE_ERROR(
                "[RenderPassSystem] Failed to create render pass for pass '{}' and MaterialClass '{}'.",
                Renderer::RenderPassTypeToString(config.type),
                config.name);
            return InvalidRenderPassHandle;
        }

        const auto handle = GetFreeHandle();
        m_RenderPasses[handle] = std::unique_ptr<Resources::IRenderPass>(renderPassPtr);
        m_RenderPassCache[key] = handle;

        return handle;
    }

    Resources::IRenderPass* RenderPassSystem::CreateRenderPass(
        const Renderer::RenderPassConfig& config,
        Renderer::PassPosition passPosition)
    {
        // Create a new RenderPass resource
        const auto rawPass = Renderer::g_RenderSystem->GetRHI()->CreateRenderPass(
            config,
            passPosition);
        if (!rawPass)
        {
            VA_ENGINE_WARN(
                "[RenderPassSystem] Failed to create render pass '{}' (Type: {}) at position '{}'.",
                config.name,
                Renderer::RenderPassTypeToString(config.type),
                static_cast<uint32_t>(passPosition));
            return nullptr;
        }

        VA_ENGINE_TRACE(
            "[RenderPassSystem] Render pass '{}' (Type: {}) created at position {}.",
            config.name,
            Renderer::RenderPassTypeToString(config.type),
            static_cast<uint32_t>(passPosition));

        return rawPass;
    }

    Resources::IRenderPass* RenderPassSystem::GetPointerFor(const RenderPassHandle handle) const
    {
        return m_RenderPasses[handle].get();
    }

    RenderPassHandle RenderPassSystem::GetFreeHandle()
    {
        // If we have a free handle in the queue, return that first
        if (!m_FreeRenderPassHandles.empty())
        {
            const auto handle = m_FreeRenderPassHandles.front();
            m_FreeRenderPassHandles.pop();
            return handle;
        }

        // Otherwise, return the next handle and increment it
        if (m_NextFreeRenderPassHandle >= m_RenderPasses.size())
        {
            m_RenderPasses.resize(m_NextFreeRenderPassHandle + 1);
        }
        const auto handle = m_NextFreeRenderPassHandle;
        m_NextFreeRenderPassHandle++;

        return handle;
    }

    void RenderPassSystem::ReleasePass(const RenderPassHandle handle)
    {
        m_FreeRenderPassHandles.push(handle);
    }

    bool Renderer::RenderPassConfig::AttachmentConfig::operator==(
        const AttachmentConfig& other) const
    {
        return name == other.name && format == other.format && loadOp == other.loadOp && storeOp ==
            other.storeOp;
    }

    bool Renderer::RenderPassConfig::operator==(const RenderPassConfig& other) const
    {
        return name == other.name && type == other.type && attachments == other.attachments;
    }

    bool RenderPassCacheKey::operator==(const RenderPassCacheKey& other) const
    {
        return config == other.config && position == other.position;
    }
} // namespace VoidArchitect
