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
              TranslateVulkanTextureFormatToEngine(image.GetFormat())),
          m_Image(std::move(image))
    {
    }
} // namespace VoidArchitect::Platform
