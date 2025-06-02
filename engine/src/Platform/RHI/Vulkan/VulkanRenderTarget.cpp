//
// Created by Michael Desmedt on 19/05/2025.
//
#include "VulkanRenderTarget.hpp"

#include "Core/Logger.hpp"
#include "VulkanRenderPass.hpp"
#include "VulkanUtils.hpp"

namespace VoidArchitect::Platform
{
    VulkanRenderTarget::VulkanRenderTarget(
        const Renderer::RenderTargetConfig& config,
        VkDevice device,
        VkAllocationCallbacks* allocator)
        : IRenderTarget(config.Name, config.Width, config.Height, config.Format, config.IsMain),
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
        : IRenderTarget(config.Name, config.Width, config.Height, config.Format, config.IsMain),
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
        InvalidateFramebuffers();
    }

    VkFramebuffer VulkanRenderTarget::GetFramebuffer(uint32_t imageIndex) const
    {
        if (imageIndex < m_Framebuffers.size())
            return m_Framebuffers[imageIndex];

        if (imageIndex > 0)
        {
            VA_ENGINE_WARN(
                "[VulkanRenderTarget] Invalid framebuffer index {} for target '{}'.",
                imageIndex,
                m_Name);
        }

        return VK_NULL_HANDLE;
    }

    bool VulkanRenderTarget::HasValidFramebuffers() const
    {
        return !m_Framebuffers.empty() && m_Framebuffers[0] != VK_NULL_HANDLE;
    }

    void VulkanRenderTarget::InvalidateFramebuffers()
    {
        for (const auto framebuffer : m_Framebuffers)
        {
            if (framebuffer != VK_NULL_HANDLE)
            {
                vkDestroyFramebuffer(m_Device, framebuffer, m_Allocator);
            }
        }
        m_Framebuffers.clear();
        VA_ENGINE_DEBUG("[VulkanRenderTarget] Framebuffers invalidated for target '{}'.", m_Name);
    }

    void VulkanRenderTarget::CreateFramebufferForImage(
        const std::unique_ptr<VulkanRenderPass>& renderpass,
        const std::vector<VkImageView>& attachments,
        uint32_t imageIndex)
    {
        // Ensure the vector has the right size
        if (m_Framebuffers.size() <= imageIndex)
        {
            m_Framebuffers.resize(imageIndex + 1, VK_NULL_HANDLE);
        }

        // Destroy existing framebuffer if it exists
        if (m_Framebuffers[imageIndex] != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(m_Device, m_Framebuffers[imageIndex], m_Allocator);
        }

        // Create new framebuffer
        auto framebufferInfo = VkFramebufferCreateInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderpass->GetHandle();
        framebufferInfo.attachmentCount = attachments.size();
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = m_Width;
        framebufferInfo.height = m_Height;
        framebufferInfo.layers = 1;

        VA_VULKAN_CHECK_RESULT_CRITICAL(
            vkCreateFramebuffer(m_Device, &framebufferInfo, m_Allocator, &m_Framebuffers[imageIndex]
            ));

        VA_ENGINE_TRACE(
            "[VulkanRenderTarget] Framebuffer created for '{}' image index {}.",
            m_Name,
            imageIndex);
    }

    void VulkanRenderTarget::Release()
    {
        for (const auto framebuffer : m_Framebuffers)
        {
            if (framebuffer != VK_NULL_HANDLE)
            {
                vkDestroyFramebuffer(m_Device, framebuffer, m_Allocator);
            }
        }
        m_Framebuffers.clear();

        // NOTE : We don't own the VkImageView attachments, they're managed by VulkanSwapchain
        //          or other systems, so we don't destroy them here.
        m_Attachments.clear();

        VA_ENGINE_TRACE("[VulkanRenderTarget] Target '{}' released.", m_Name);
    }
}
