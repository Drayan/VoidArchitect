//
// Created by Michael Desmedt on 19/05/2025.
//
#pragma once
#include <vulkan/vulkan.h>

#include "Resources/RenderTarget.hpp"

namespace VoidArchitect::Renderer
{
    struct RenderTargetConfig;
}

namespace VoidArchitect::Platform
{
    class VulkanDevice;
    class VulkanRenderPass;

    class VulkanRenderTarget : public Resources::IRenderTarget
    {
    public:
        // Constructor for the main target (for the swapchain)
        VulkanRenderTarget(
            const Renderer::RenderTargetConfig& config,
            VkDevice device,
            VkAllocationCallbacks* allocator);

        // Constructor for a texture-based target
        VulkanRenderTarget(
            const Renderer::RenderTargetConfig& config,
            VkDevice device,
            VkAllocationCallbacks* allocator,
            const std::vector<VkImageView>& attachments);
        ~VulkanRenderTarget() override;

        void Resize(uint32_t width, uint32_t height) override;

        // Vulkan-specific methods
        void CreateFramebuffer(
            const std::unique_ptr<VulkanRenderPass>& renderpass,
            const std::vector<VkImageView>& attachments);
        void UpdateAttachments(const std::vector<VkImageView>& attachments);

        [[nodiscard]] VkFramebuffer GetFramebuffer() const { return m_Framebuffer; };

    protected:
        void Release() override;

    private:
        void CreateFramebufferInternal(
            VkRenderPass renderPass,
            const std::vector<VkImageView>& attachments);

        VkDevice m_Device;
        VkAllocationCallbacks* m_Allocator;
        VkFramebuffer m_Framebuffer = VK_NULL_HANDLE;

        std::vector<VkImageView> m_Attachments;

        // For texture-based targets, we might own the image views.
        bool m_OwnsAttachments = false;
    };
}
