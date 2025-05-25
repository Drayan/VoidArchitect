//
// Created by Michael Desmedt on 22/05/2025.
//
#include "VulkanTexture.hpp"

#include "VulkanBuffer.hpp"

namespace VoidArchitect::Platform
{
    VulkanTexture2D::VulkanTexture2D(
        const VulkanRHI& rhi,
        const std::unique_ptr<VulkanDevice>& device,
        VkAllocationCallbacks* allocator,
        const std::string& name,
        const uint32_t width,
        const uint32_t height,
        const uint8_t channels,
        const bool hasTransparency,
        const std::vector<uint8_t>& data)
        : Texture2D(name, width, height, channels, hasTransparency),
          m_Generation(0),
          m_Image{},
          m_Sampler{},
          m_Device(device->GetLogicalDeviceHandle()),
          m_Allocator(allocator)
    {
        // NOTE: Assume 8 bits channel.
        // TODO: Support configurable format.
        VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM;

        // Create the staging buffer and image.
        auto staging = VulkanStagingBuffer(rhi, device, m_Allocator, data);
        m_Image = VulkanImage(
            rhi,
            device,
            m_Allocator,
            m_Width,
            m_Height,
            imageFormat,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        // Load the data from the staging buffer to the image.
        VulkanCommandBuffer cmdBuf;
        VulkanCommandBuffer::SingleUseBegin(device, device->GetGraphicsCommandPool(), cmdBuf);

        m_Image.TransitionLayout(
            device,
            cmdBuf,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        m_Image.CopyFromBuffer(cmdBuf, staging);
        m_Image.TransitionLayout(
            device,
            cmdBuf,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        VulkanCommandBuffer::SingleUseEnd(cmdBuf, device->GetGraphicsQueueHandle(), nullptr);

        // Create the sampler for the texture
        auto samplerInfo = VkSamplerCreateInfo{};
        // TODO Make this configurable.
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = 16;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        VA_VULKAN_CHECK_RESULT_WARN(
            vkCreateSampler(m_Device, &samplerInfo, m_Allocator, &m_Sampler));

        VA_ENGINE_TRACE("[VulkanTexture2D] Texture created.");
    }

    VulkanTexture2D::~VulkanTexture2D()
    {
        VulkanTexture2D::Release();
    }

    void VulkanTexture2D::Release()
    {
        if (m_Sampler)
        {
            vkDeviceWaitIdle(m_Device);
            vkDestroySampler(m_Device, m_Sampler, m_Allocator);
            m_Sampler = VK_NULL_HANDLE;
            m_Image = {};

            VA_ENGINE_TRACE("[VulkanTexture2D] Texture destroyed.");
        }
    }
} // VoidArchitect
