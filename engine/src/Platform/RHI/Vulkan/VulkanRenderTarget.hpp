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
        [[nodiscard]] bool HasValidFramebuffers() const;
        void InvalidateFramebuffers();
        void CreateFramebufferForImage(
            VkRenderPass renderpass,
            const std::vector<VkImageView>& attachments,
            uint32_t imageIndex);

        [[nodiscard]] VkFramebuffer GetFramebuffer(uint32_t imageIndex) const;

    protected:
        void Release() override;

    private:
        VkDevice m_Device;
        VkAllocationCallbacks* m_Allocator;

        std::vector<VkFramebuffer> m_Framebuffers;
        std::vector<VkImageView> m_Attachments;

        // For texture-based targets, we might own the image views.
        bool m_OwnsAttachments = false;
    };
} // namespace VoidArchitect::Platform
