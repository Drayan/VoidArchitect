//
// Created by Michael Desmedt on 19/05/2025.
//
#pragma once

#include "VulkanImage.hpp"
#include "Resources/RenderTarget.hpp"

namespace VoidArchitect::Renderer
{
    struct RenderTargetConfig;
}

namespace VoidArchitect::Platform
{
    class VulkanImage;
    class VulkanDevice;
    class VulkanRenderPass;

    class VulkanRenderTarget : public Resources::IRenderTarget
    {
    public:
        VulkanRenderTarget(const std::string& name, VulkanImage&& image);
        ~VulkanRenderTarget() override = default;

        VulkanImage& GetImage() { return m_Image; }
        VkImageView GetImageView() const { return m_Image.GetView(); }

    private:
        bool IsDepthTarget(VkFormat format) const;

        VulkanImage m_Image;
    };
} // namespace VoidArchitect::Platform
