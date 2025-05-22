//
// Created by Michael Desmedt on 22/05/2025.
//
#include "Texture.hpp"

#include "Core/Logger.hpp"
#include "Systems/Renderer/RenderCommand.hpp"

namespace VoidArchitect::Resources
{
    ITexture::ITexture(
        const uint32_t width,
        const uint32_t height,
        const uint8_t channelCount,
        const bool hasTransparency)
        : m_Handle{},
          m_Width(width),
          m_Height(height),
          m_ChannelCount(channelCount),
          m_HasTransparency(hasTransparency),
          m_Generation(std::numeric_limits<uint32_t>::max())
    {
    }

    std::shared_ptr<Texture2D> Texture2D::Create(const std::string& name)
    {
        return Renderer::RenderCommand::CreateTexture2D(name);
    }

    Texture2D::Texture2D(
        const uint32_t width,
        const uint32_t height,
        const uint8_t channelCount,
        const bool hasTransparency)
        : ITexture(width, height, channelCount, hasTransparency)
    {
    }
} // VoidArchitect
