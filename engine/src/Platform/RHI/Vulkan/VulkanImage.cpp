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
        const VkImageAspectFlags aspect,
        const bool createImageView)
        : m_Device(device->GetLogicalDeviceHandle()),
          m_Allocator(allocator),
          m_ExternallyAllocated(true),
          m_Image(image),
          m_ImageView{}
    {
        if (createImageView)
        {
            CreateImageView(image, format, aspect);
        }
        VA_ENGINE_TRACE("[VulkanImage] Image created.");
    }

    VulkanImage::VulkanImage(
        const VulkanRHI& rhi,
        const std::unique_ptr<VulkanDevice>& device,
        VkAllocationCallbacks* allocator,
        const uint32_t width,
        const uint32_t height,
        const VkFormat format,
        const VkImageAspectFlags aspect,
        const VkImageTiling tiling,
        const VkImageUsageFlags usage,
        const VkMemoryPropertyFlags memoryFlags,
        const bool createImageView)
        : m_Device(device->GetLogicalDeviceHandle()),
          m_Allocator(allocator),
          m_Width(width),
          m_Height(height),
          m_Format(format),
          m_Image{},
          m_ImageView{}
    {
        CreateImage(rhi, width, height, format, tiling, usage, memoryFlags);
        if (createImageView)
        {
            CreateImageView(m_Image, format, aspect);
        }
        VA_ENGINE_TRACE("[VulkanImage] Image created.");
    }

    VulkanImage::VulkanImage(VulkanImage&& other) noexcept
        : m_Device(other.m_Device),
          m_Allocator(other.m_Allocator),
          m_Width(other.m_Width),
          m_Height(other.m_Height),
          m_Format(other.m_Format),
          m_Image(other.m_Image),
          m_ImageView(other.m_ImageView),
          m_Memory(other.m_Memory)
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
            m_Width = other.m_Width;
            m_Height = other.m_Height;
            m_Format = other.m_Format;
            m_Image = other.m_Image;
            m_ImageView = other.m_ImageView;
            m_Memory = other.m_Memory;

            other.InvalidateResources();
        }

        return *this;
    }

    void VulkanImage::CreateImage(
        const VulkanRHI& rhi,
        const uint32_t width,
        const uint32_t height,
        const VkFormat format,
        const VkImageTiling tiling,
        const VkImageUsageFlags usage,
        const VkMemoryPropertyFlags memoryFlags)
    {
        auto createInfo = VkImageCreateInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        createInfo.imageType = VK_IMAGE_TYPE_2D;
        createInfo.extent.width = width;
        createInfo.extent.height = height;
        createInfo.extent.depth = 1; // TODO Support configurable depth
        createInfo.mipLevels = 4; // TODO Support mip mapping
        createInfo.arrayLayers = 1; // TODO Support number of layers in the image
        createInfo.format = format;
        createInfo.tiling = tiling;
        createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        createInfo.usage = usage;
        createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(m_Device, &createInfo, m_Allocator, &m_Image) != VK_SUCCESS)
        {
            VA_ENGINE_WARN("[VulkanImage] Failed to create image.");
            return;
        }

        // Query memory requirements
        VkMemoryRequirements memRequirements;
        if (vkGetImageMemoryRequirements == nullptr)
        {
            VA_ENGINE_CRITICAL("[VulkanImage] Failed to get vkGetImageMemoryRequirements.");
            return;
        }
        vkGetImageMemoryRequirements(m_Device, m_Image, &memRequirements);
        const int32_t memoryTypeIndex = rhi.FindMemoryIndex(
            memRequirements.memoryTypeBits,
            memoryFlags);
        if (memoryTypeIndex == -1)
        {
            VA_ENGINE_CRITICAL("[VulkanImage] Failed to find memory type index.");
            return;
        }

        // Allocate memory
        auto allocateInfo = VkMemoryAllocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocateInfo.allocationSize = memRequirements.size;
        allocateInfo.memoryTypeIndex = memoryTypeIndex;

        if (vkAllocateMemory(m_Device, &allocateInfo, m_Allocator, &m_Memory) != VK_SUCCESS)
        {
            VA_ENGINE_WARN("[VulkanImage] Failed to allocate memory.");
            return;
        }

        // Bind the memory, TODO Configurable memory offset
        if (vkBindImageMemory(m_Device, m_Image, m_Memory, 0) != VK_SUCCESS)
        {
            VA_ENGINE_WARN("[VulkanImage] Failed to bind memory.");
            return;
        }
    }

    void VulkanImage::CreateImageView(
        const VkImage image,
        const VkFormat format,
        const VkImageAspectFlags aspect)
    {
        auto createInfo = VkImageViewCreateInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = image;
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = format;
        createInfo.subresourceRange.aspectMask = aspect;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_A;

        if (vkCreateImageView(
            m_Device,
            &createInfo,
            m_Allocator,
            &m_ImageView) != VK_SUCCESS)
        {
            VA_ENGINE_WARN("[VulkanImage] Failed to create ImageView.");
        }
    }

    void VulkanImage::InvalidateResources()
    {
        m_Device = VK_NULL_HANDLE;
        m_Allocator = nullptr;
        m_Image = VK_NULL_HANDLE;
        m_ImageView = VK_NULL_HANDLE;
        m_Memory = VK_NULL_HANDLE;
        m_Width = 0;
        m_Height = 0;
        m_Format = VK_FORMAT_UNDEFINED;
    }

    VulkanImage::~VulkanImage()
    {
        if (m_ImageView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(m_Device, m_ImageView, m_Allocator);
            VA_ENGINE_TRACE("[VulkanImage] ImageView destroyed.");
        }

        if (m_Memory != VK_NULL_HANDLE)
        {
            vkFreeMemory(m_Device, m_Memory, m_Allocator);
            VA_ENGINE_TRACE("[VulkanImage] Memory released.");
        }

        if (m_Image != VK_NULL_HANDLE && !m_ExternallyAllocated)
        {
            vkDestroyImage(m_Device, m_Image, m_Allocator);
            VA_ENGINE_TRACE("[VulkanImage] Image destroyed.");
        }
    }
} // VoidArchitect
