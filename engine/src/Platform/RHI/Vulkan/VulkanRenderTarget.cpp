//
// Created by Michael Desmedt on 19/05/2025.
//
#include "VulkanRenderTarget.hpp"

#include "VulkanUtils.hpp"
#include "Core/Logger.hpp"

namespace VoidArchitect::Platform
{
    VulkanRenderTarget::VulkanRenderTarget(const std::string& name, VulkanImage&& image)
        : IRenderTarget(
              name,
              image.GetWidth(),
              image.GetHeight(),
              TranslateVulkanTextureFormatToEngine(image.GetFormat()),
              IsDepthTarget(image.GetFormat())),
          m_Image(std::move(image))
    {
    }

    bool VulkanRenderTarget::IsDepthTarget(const VkFormat format) const
    {
        switch (format)
        {
            default:
                return false;

            case VK_FORMAT_D32_SFLOAT:
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
            case VK_FORMAT_D24_UNORM_S8_UINT:
                return true;
        }
    }
} // namespace VoidArchitect::Platform
