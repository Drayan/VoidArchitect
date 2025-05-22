//
// Created by Michael Desmedt on 22/05/2025.
//
#pragma once

#include "VulkanImage.hpp"
#include "Resources/Texture.hpp"

#include <vulkan/vulkan.h>

namespace VoidArchitect::Platform
{
    class VulkanRHI;
    class VulkanDevice;

    class VulkanTexture2D : public Resources::Texture2D
    {
    public:
        VulkanTexture2D(
            const VulkanRHI& rhi,
            const std::unique_ptr<VulkanDevice>& device,
            VkAllocationCallbacks* allocator,
            const std::vector<uint8_t>& data);
        ~VulkanTexture2D();

    private:
        VulkanImage m_Image;
        VkSampler m_Sampler;

        VkDevice m_Device;
        VkAllocationCallbacks* m_Allocator;
    };
}
