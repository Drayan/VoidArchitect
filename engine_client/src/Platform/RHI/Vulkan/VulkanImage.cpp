//
// Created by Michael Desmedt on 17/05/2025.
//
#include "VulkanImage.hpp"

#include "VulkanRhi.hpp"
#include "Core/Logger.hpp"

namespace VoidArchitect::Platform
{
    VulkanImage::VulkanImage(
        const std::unique_ptr<VulkanDevice>& device,
        VkAllocationCallbacks* allocator,
        const VkImage image,
        const VkFormat format,
        const bool createImageView)
        : m_Device(device->GetLogicalDeviceHandle()),
          m_Allocator(allocator),
          m_Image(image),
          m_ImageView{}
    {
        if (createImageView)
        {
            auto createInfo = VkImageViewCreateInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = image;
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = format;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;
            createInfo.components.r = VK_COMPONENT_SWIZZLE_R;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_G;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_B;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_A;

            if (vkCreateImageView(
                device->GetLogicalDeviceHandle(),
                &createInfo,
                m_Allocator,
                &m_ImageView) != VK_SUCCESS)
            {
                VA_ENGINE_WARN("[VulkanImage] Failed to create ImageView.");
                return;
            }
        }
    }

    VulkanImage::VulkanImage(VulkanImage&& other) noexcept
        : m_Device(other.m_Device),
          m_Allocator(other.m_Allocator),
          m_Image(other.m_Image),
          m_ImageView(other.m_ImageView)
    {
        other.InvalidateResources();
    }

    VulkanImage& VulkanImage::operator=(VulkanImage&& other) noexcept
    {
        if (this != &other)
        {
            this->~VulkanImage();

            m_Device = other.m_Device;
            m_Allocator = other.m_Allocator;
            m_Image = other.m_Image;
            m_ImageView = other.m_ImageView;

            other.InvalidateResources();
        }

        return *this;
    }

    void VulkanImage::InvalidateResources()
    {
        m_Device = VK_NULL_HANDLE;
        m_Allocator = nullptr;
        m_Image = VK_NULL_HANDLE;
        m_ImageView = VK_NULL_HANDLE;
    }

    VulkanImage::~VulkanImage()
    {
        if (m_ImageView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(m_Device, m_ImageView, m_Allocator);
            VA_ENGINE_TRACE("[VulkanImage] ImageView destroyed.");
        }
    }
} // VoidArchitect
