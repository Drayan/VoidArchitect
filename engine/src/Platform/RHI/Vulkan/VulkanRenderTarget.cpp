//
// Created by Michael Desmedt on 19/05/2025.
//
#include "VulkanRenderTarget.hpp"

#include "Core/Logger.hpp"
#include "VulkanRenderpass.hpp"
#include "VulkanUtils.hpp"

namespace VoidArchitect::Platform
{
    VulkanRenderTarget::VulkanRenderTarget(
        const Renderer::RenderTargetConfig& config,
        VkDevice device,
        VkAllocationCallbacks* allocator)
        : IRenderTarget(config.Name, config.Width, config.Height, config.Format, config.isMain),
          m_Device(device),
          m_Allocator(allocator)
    {
        // For main targets, a framebuffer will be created later when attachments are provided.
        // This constructor is mainly for storing the configuration.
        VA_ENGINE_TRACE(
            "[VulkanRenderTarget] Main target '{}' created ({}x{}).",
            config.Name,
            config.Width,
            config.Height);
    }

    VulkanRenderTarget::VulkanRenderTarget(
        const Renderer::RenderTargetConfig& config,
        VkDevice device,
        VkAllocationCallbacks* allocator,
        const std::vector<VkImageView>& attachments)
        : IRenderTarget(config.Name, config.Width, config.Height, config.Format, config.isMain),
          m_Device(device),
          m_Allocator(allocator),
          m_Attachments(attachments)
    {
        // For texture-based targets, we have attachments immediately,
        // But we still need a render pass to create the framebuffer.
        VA_ENGINE_TRACE(
            "[VulkanRenderTarget] Texture target '{}' created ({}x{}) with {} attachments.",
            config.Name,
            config.Width,
            config.Height,
            attachments.size());
    }

    VulkanRenderTarget::~VulkanRenderTarget()
    {
        Release();
    }

    void VulkanRenderTarget::Resize(uint32_t width, uint32_t height)
    {
        if (m_Width == width && m_Height == height)
            return;

        VA_ENGINE_TRACE(
            "[VulkanRenderTarget] Resizing '{}' from {}x{} to {}x{}.",
            m_Name,
            m_Width,
            m_Height,
            width,
            height);

        m_Width = width;
        m_Height = height;

        // If we have a framebuffer, we need to recreate it with new dimensions.
        if (m_Framebuffer != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(m_Device, m_Framebuffer, m_Allocator);
            m_Framebuffer = VK_NULL_HANDLE;

            // NOTE : The caller (typically VulkanSwapchain) will need to call
            // CreateFramebuffer() again with updated attachments.
            VA_ENGINE_DEBUG(
                "[VulkanRenderTarget] Framebuffer destroyed for resize, need recreation.");
        }
    }

    void VulkanRenderTarget::CreateFramebuffer(
        const std::unique_ptr<VulkanRenderpass>& renderpass,
        const std::vector<VkImageView>& attachments)
    {
        // Store attachment for potential future recreation
        m_Attachments = attachments;

        CreateFramebufferInternal(renderpass->GetHandle(), attachments);
    }

    void VulkanRenderTarget::UpdateAttachments(const std::vector<VkImageView>& attachments)
    {
        m_Attachments = attachments;

        // Destroy an existing framebuffer if it exists
        if (m_Framebuffer != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(m_Device, m_Framebuffer, m_Allocator);
            m_Framebuffer = VK_NULL_HANDLE;
        }

        // NOTE : CreateFramebuffer() must be called again to recreate with new attachments
        VA_ENGINE_DEBUG(
            "[VulkanRenderTarget] Attachments updated for '{}', framebuffer needs recreation.",
            m_Name);
    }

    void VulkanRenderTarget::CreateFramebufferInternal(
        VkRenderPass renderPass,
        const std::vector<VkImageView>& attachments)
    {
        // Destroy an existing framebuffer if it exists
        if (m_Framebuffer != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(m_Device, m_Framebuffer, m_Allocator);
        }

        auto framebufferInfo = VkFramebufferCreateInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = attachments.size();
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = m_Width;
        framebufferInfo.height = m_Height;
        framebufferInfo.layers = 1;

        VA_VULKAN_CHECK_RESULT_CRITICAL(
            vkCreateFramebuffer(m_Device, &framebufferInfo, m_Allocator, &m_Framebuffer));

        VA_ENGINE_TRACE(
            "[VulkanRenderTarget] Framebuffer created for '{}' ({}x{}) with {} attachments.",
            m_Name,
            m_Width,
            m_Height,
            attachments.size());
    }

    void VulkanRenderTarget::Release()
    {
        if (m_Framebuffer != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(m_Device, m_Framebuffer, m_Allocator);
            m_Framebuffer = VK_NULL_HANDLE;
            VA_ENGINE_TRACE("[VulkanRenderTarget] Framebuffer destroyed for '{}'.", m_Name);
        }

        // NOTE : We don't own the VkImageView attachments, they're managed by VulkanSwapchain
        //          or other systems, so we don't destroy them here.
        m_Attachments.clear();
    }
}
