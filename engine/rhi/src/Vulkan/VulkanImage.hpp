//
// Created by Michael Desmedt on 17/05/2025.
//
#pragma once
#include <vulkan/vulkan.h>

#include "VulkanRhi.hpp"

namespace VoidArchitect::Platform
{
    class VulkanBuffer;
    class VulkanCommandBuffer;
    class VulkanDevice;

    class VulkanImage
    {
    public:
        VulkanImage() = default;
        VulkanImage(
            const std::unique_ptr<VulkanDevice>& device,
            VkAllocationCallbacks* allocator,
            VkImage image,
            VkFormat format,
            VkImageAspectFlags aspect,
            bool createImageView = true);
        VulkanImage(
            const std::unique_ptr<VulkanDevice>& device,
            VkAllocationCallbacks* allocator,
            uint32_t width,
            uint32_t height,
            VkFormat format,
            VkImageAspectFlags aspect,
            VkImageTiling tiling,
            VkImageUsageFlags usage,
            VkMemoryPropertyFlags memoryFlags,
            bool createImageView = true);

        VulkanImage(VulkanImage&& other) noexcept;
        VulkanImage(const VulkanImage& other) = delete;
        ~VulkanImage();

        VulkanImage& operator=(VulkanImage&& other) noexcept;
        VulkanImage& operator=(const VulkanImage& other) = delete;

        void TransitionLayout(
            const std::unique_ptr<VulkanDevice>& device,
            const VulkanCommandBuffer& cmdBuf,
            VkImageLayout oldLayout,
            VkImageLayout newLayout) const;
        void CopyFromBuffer(const VulkanCommandBuffer& cmdBuf, const VulkanBuffer& buffer) const;

        [[nodiscard]] VkImageView GetView() const { return m_ImageView; }
        uint32_t GetWidth() const { return m_Width; }
        uint32_t GetHeight() const { return m_Height; }
        VkFormat GetFormat() const { return m_Format; }

    private:
        void CreateImage(
            const std::unique_ptr<VulkanDevice>& device,
            uint32_t width,
            uint32_t height,
            VkFormat format,
            VkImageTiling tiling,
            VkImageUsageFlags usage,
            VkMemoryPropertyFlags memoryFlags);
        void CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect);
        void InvalidateResources();

        VkDevice m_Device;
        VkAllocationCallbacks* m_Allocator;

        uint32_t m_Width;
        uint32_t m_Height;
        VkFormat m_Format;

        bool m_ExternallyAllocated = false;
        VkImage m_Image;
        VkImageView m_ImageView;
        VkDeviceMemory m_Memory;
    };
} // namespace VoidArchitect::Platform
