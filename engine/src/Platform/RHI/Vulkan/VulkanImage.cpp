//
// Created by Michael Desmedt on 17/05/2025.
//
#include "VulkanImage.hpp"

#include "Core/Logger.hpp"
#include "VulkanBuffer.hpp"
#include "VulkanCommandBuffer.hpp"
#include "VulkanDevice.hpp"
#include "VulkanUtils.hpp"

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
          m_Width(),
          m_Height(),
          m_Format(format),
          m_ExternallyAllocated(true),
          m_Image(image),
          m_ImageView{},
          m_Memory{}
    {
        if (createImageView)
        {
            CreateImageView(image, format, aspect);
        }
        VA_ENGINE_TRACE("[VulkanImage] Image created.");
    }

    VulkanImage::VulkanImage(
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
          m_ImageView{},
          m_Memory{}
    {
        CreateImage(device, width, height, format, tiling, usage, memoryFlags);
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

    void VulkanImage::TransitionLayout(
        const std::unique_ptr<VulkanDevice>& device,
        const VulkanCommandBuffer& cmdBuf,
        const VkImageLayout oldLayout,
        const VkImageLayout newLayout) const
    {
        auto barrier = VkImageMemoryBarrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = device->GetGraphicsFamily().value();
        barrier.dstQueueFamilyIndex = device->GetGraphicsFamily().value();
        barrier.image = m_Image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED
            && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (
            oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
            && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else
        {
            VA_ENGINE_CRITICAL("[VulkanImage] Unsupported layout transition.");
            return;
        }

        vkCmdPipelineBarrier(
            cmdBuf.GetHandle(),
            sourceStage,
            destinationStage,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrier);
    }

    void VulkanImage::CopyFromBuffer(
        const VulkanCommandBuffer& cmdBuf,
        const VulkanBuffer& buffer) const
    {
        auto region = VkBufferImageCopy{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {m_Width, m_Height, 1};

        vkCmdCopyBufferToImage(
            cmdBuf.GetHandle(),
            buffer.GetHandle(),
            m_Image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region);
    }

    void VulkanImage::CreateImage(
        const std::unique_ptr<VulkanDevice>& device,
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

        VA_VULKAN_CHECK_RESULT_WARN(
            vkCreateImage(m_Device, &createInfo, m_Allocator, &m_Image));

        // Query memory requirements
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(m_Device, m_Image, &memRequirements);
        const int32_t memoryTypeIndex =
            device->FindMemoryIndex(memRequirements.memoryTypeBits, memoryFlags);
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

        VA_VULKAN_CHECK_RESULT_CRITICAL(
            vkAllocateMemory(m_Device, &allocateInfo, m_Allocator, &
                m_Memory));

        // Bind the memory, TODO Configurable memory offset
        VA_VULKAN_CHECK_RESULT_WARN(
            vkBindImageMemory(m_Device, m_Image, m_Memory, 0));
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

        VA_VULKAN_CHECK_RESULT_WARN(
            vkCreateImageView(m_Device, &createInfo, m_Allocator, &
                m_ImageView));
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
} // namespace VoidArchitect::Platform
