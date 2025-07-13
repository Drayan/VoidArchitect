//
// Created by Michael Desmedt on 22/05/2025.
//
#pragma once

#include "Resources/Texture.hpp"
#include "VulkanImage.hpp"

#include <vulkan/vulkan.h>

namespace VoidArchitect::Platform
{
    class VulkanRHI;
    class VulkanDevice;

    class VulkanTexture2D : public Resources::Texture2D
    {
    public:
        VulkanTexture2D(
            const std::unique_ptr<VulkanDevice>& device,
            VkAllocationCallbacks* allocator,
            const std::string& name,
            uint32_t width,
            uint32_t height,
            uint8_t channels,
            bool hasTransparency,
            const VAArray<uint8_t>& data);
        ~VulkanTexture2D() override;

        VkImageView GetImageView() const { return m_Image.GetView(); }
        VkSampler GetSampler() const { return m_Sampler; }

    protected:
        void Release() override;

    private:
        VulkanImage m_Image;
        VkSampler m_Sampler;

        VkDevice m_Device;
        VkAllocationCallbacks* m_Allocator;
    };
} // namespace VoidArchitect::Platform
