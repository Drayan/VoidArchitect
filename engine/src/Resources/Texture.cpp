//
// Created by Michael Desmedt on 22/05/2025.
//
#include "Texture.hpp"

#include <stb_image.h>

#include <utility>

#include "Core/Logger.hpp"
#include "Platform/RHI/IRenderingHardware.hpp"
#include "Systems/Renderer/RenderCommand.hpp"

namespace VoidArchitect::Resources
{
    ITexture::ITexture(
        std::string name,
        const uint32_t width,
        const uint32_t height,
        const uint8_t channelCount,
        const bool hasTransparency)
        : m_Name(std::move(name)),
          m_Handle{},
          m_Width(width),
          m_Height(height),
          m_ChannelCount(channelCount),
          m_HasTransparency(hasTransparency)
    {
    }

    Texture2D::Texture2D(
        const std::string& name,
        const uint32_t width,
        const uint32_t height,
        const uint8_t channelCount,
        const bool hasTransparency)
        : ITexture(name, width, height, channelCount, hasTransparency)
    {
    }
} // namespace VoidArchitect::Resources
