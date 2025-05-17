//
// Created by Michael Desmedt on 17/05/2025.
//
#pragma once
#include <vulkan/vulkan.h>

namespace VoidArchitect::Platform
{
    class VulkanDevice;

    class VulkanImage
    {
    public:
        VulkanImage(
            const std::unique_ptr<VulkanDevice>& device,
            VkAllocationCallbacks* allocator,
            VkImage image,
            VkFormat format,
            bool createImageView = true);
        VulkanImage(VulkanImage&& other) noexcept;
        VulkanImage(const VulkanImage& other) = delete;
        ~VulkanImage();

        VulkanImage& operator=(VulkanImage&& other) noexcept;
        VulkanImage& operator=(const VulkanImage& other) = delete;

    private:
        void InvalidateResources();

        VkDevice m_Device;
        VkAllocationCallbacks* m_Allocator;

        VkImage m_Image;
        VkImageView m_ImageView;
    };
} // VoidArchitect
