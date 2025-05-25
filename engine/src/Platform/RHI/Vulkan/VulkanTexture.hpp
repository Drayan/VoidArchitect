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
            const std::string& name,
            uint32_t width,
            uint32_t height,
            uint8_t channels,
            bool hasTransparency,
            const std::vector<uint8_t>& data);
        ~VulkanTexture2D() override;

        VkImageView GetImageView() const { return m_Image.GetView(); }
        VkSampler GetSampler() const { return m_Sampler; }
        uint32_t GetGeneration() const { return m_Generation; }

        void SetGeneration(const uint32_t generation) { m_Generation = generation; }

    protected:
        void Release() override;

    private:
        uint32_t m_Generation;
        VulkanImage m_Image;
        VkSampler m_Sampler;

        VkDevice m_Device;
        VkAllocationCallbacks* m_Allocator;
    };
}
