//
// Created by Michael Desmedt on 09/06/2025.
//
#include "VulkanRenderTargetSystem.hpp"

#include <VoidArchitect/Engine/Common/Logger.hpp>

#include "VulkanRenderTarget.hpp"
#include "VulkanResourceFactory.hpp"

namespace VoidArchitect::Platform
{
    VulkanRenderTargetSystem::VulkanRenderTargetSystem(
        const std::unique_ptr<VulkanResourceFactory>& factory)
        : m_ResourceFactory(factory)
    {
        m_RenderTargets.reserve(10);
    }

    Resources::RenderTargetHandle VulkanRenderTargetSystem::CreateRenderTarget(
        const Renderer::RenderTargetConfig& config)
    {
        const auto target = static_cast<VulkanRenderTarget*>(m_ResourceFactory->CreateRenderTarget(
            config));
        const auto handle = GetFreeHandle();
        m_RenderTargets[handle] = std::unique_ptr<VulkanRenderTarget>(target);

        VA_ENGINE_TRACE(
            "[VulkanRenderTargetSystem] Created render target '{}' with handle: {}.",
            config.name,
            handle);
        return handle;
    }

    Resources::RenderTargetHandle VulkanRenderTargetSystem::CreateRenderTarget(
        const std::string& name,
        const VkImage nativeImage,
        const VkFormat format)
    {
        const auto target = static_cast<VulkanRenderTarget*>(m_ResourceFactory->CreateRenderTarget(
            name,
            nativeImage,
            format));
        const auto handle = GetFreeHandle();
        m_RenderTargets[handle] = std::unique_ptr<VulkanRenderTarget>(target);

        VA_ENGINE_TRACE(
            "[VulkanRenderTargetSystem] Created render target '{}' with handle: {}.",
            name,
            handle);
        return handle;
    }

    void VulkanRenderTargetSystem::ReleaseRenderTarget(const Resources::RenderTargetHandle handle)
    {
        m_RenderTargets[handle] = nullptr;
        m_FreeRenderTargetsHandles.push(handle);
    }

    Resources::IRenderTarget* VulkanRenderTargetSystem::GetPointerFor(
        const Resources::RenderTargetHandle handle) const
    {
        return m_RenderTargets[handle].get();
    }

    Resources::RenderTargetHandle VulkanRenderTargetSystem::GetFreeHandle()
    {
        // Check if we have a free handle in the queue
        if (!m_FreeRenderTargetsHandles.empty())
        {
            const auto handle = m_FreeRenderTargetsHandles.front();
            m_FreeRenderTargetsHandles.pop();
            return handle;
        }

        // Otherwise, return the next handle and increment it
        if (m_NextFreeRenderTargetHandle >= m_RenderTargets.size())
        {
            m_RenderTargets.resize(m_NextFreeRenderTargetHandle + 1);
        }
        return m_NextFreeRenderTargetHandle++;
    }
}
