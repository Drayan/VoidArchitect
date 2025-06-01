//
// Created by Michael Desmedt on 19/05/2025.
//
#include "VulkanFramebuffer.hpp"

#include <ranges>

#include "Core/Logger.hpp"
#include "VulkanRenderpass.hpp"
#include "VulkanUtils.hpp"

namespace VoidArchitect::Platform
{
    VulkanFramebuffer::VulkanFramebuffer(
        const VkDevice device,
        VkAllocationCallbacks* allocator,
        const std::unique_ptr<VulkanRenderpass>& renderpass,
        const uint32_t width,
        const uint32_t height,
        const std::vector<VkImageView>& attachments)
        : m_Device(device),
          m_Allocator(allocator)
    {
        auto framebufferInfo = VkFramebufferCreateInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderpass->GetHandle();
        framebufferInfo.attachmentCount = attachments.size();
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = width;
        framebufferInfo.height = height;
        framebufferInfo.layers = 1;

        VA_VULKAN_CHECK_RESULT_CRITICAL(
            vkCreateFramebuffer(m_Device, &framebufferInfo, m_Allocator, &m_Framebuffer));
    }

    VulkanFramebuffer::~VulkanFramebuffer()
    {
        if (m_Framebuffer != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(m_Device, m_Framebuffer, m_Allocator);
            VA_ENGINE_TRACE("[VulkanFramebuffer] Framebuffer destroyed.");
        }
    }
}
