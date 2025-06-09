//
// Created by Michael Desmedt on 09/06/2025.
//
#pragma once
#include <vulkan/vulkan_core.h>

#include "VulkanImage.hpp"
#include "Systems/Renderer/RendererTypes.hpp"
#include "Resources/RenderTarget.hpp"

namespace VoidArchitect
{
    namespace Platform
    {
        class VulkanResourceFactory;

        class VulkanRenderTargetSystem
        {
        public:
            VulkanRenderTargetSystem(const std::unique_ptr<VulkanResourceFactory>& factory);
            ~VulkanRenderTargetSystem() = default;

            Resources::RenderTargetHandle CreateRenderTarget(
                const Renderer::RenderTargetConfig& config);

            Resources::RenderTargetHandle CreateRenderTarget(
                const std::string& name,
                VkImage nativeImage,
                VkFormat format);

            void ReleaseRenderTarget(Resources::RenderTargetHandle handle);

            Resources::IRenderTarget* GetPointerFor(Resources::RenderTargetHandle handle) const;

        private:
            Resources::RenderTargetHandle GetFreeHandle();
            const std::unique_ptr<VulkanResourceFactory>& m_ResourceFactory;

            std::queue<Resources::RenderTargetHandle> m_FreeRenderTargetsHandles;
            Resources::RenderTargetHandle m_NextFreeRenderTargetHandle = 0;

            VAArray<std::unique_ptr<Resources::IRenderTarget>> m_RenderTargets;
        };

        inline std::unique_ptr<VulkanRenderTargetSystem> g_VkRenderTargetSystem;
    } // Platform
} // VoidArchitect
