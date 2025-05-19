//
// Created by Michael Desmedt on 19/05/2025.
//
#pragma once
#include <vulkan/vulkan.h>

namespace VoidArchitect::Platform
{
    class VulkanDevice;
    class VulkanRenderpass;

    class VulkanFramebuffer
    {
    public:
        // VulkanFramebuffer(
        //     const std::unique_ptr<VulkanDevice>& device,
        //     VkAllocationCallbacks* allocator,
        //     const std::unique_ptr<VulkanRenderpass>& renderpass
        // );
        VulkanFramebuffer(
            VkDevice device,
            VkAllocationCallbacks* allocator,
            const std::unique_ptr<VulkanRenderpass>& renderpass,
            uint32_t width,
            uint32_t height,
            const std::vector<VkImageView>& attachments);
        ~VulkanFramebuffer();
        VkFramebuffer GetHandle() const { return m_Framebuffer; };

    private:
        VkDevice m_Device;
        VkAllocationCallbacks* m_Allocator;
        VkFramebuffer m_Framebuffer;
    };
} // VoidArchitect
