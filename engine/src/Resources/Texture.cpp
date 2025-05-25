//
// Created by Michael Desmedt on 22/05/2025.
//
#include "Texture.hpp"

#include <stb_image.h>

#include "Core/Logger.hpp"
#include "Platform/RHI/IRenderingHardware.hpp"
#include "Systems/Renderer/RenderCommand.hpp"

namespace VoidArchitect::Resources
{
    ITexture::ITexture(
        const std::string& name,
        const uint32_t width,
        const uint32_t height,
        const uint8_t channelCount,
        const bool hasTransparency)
        : m_Name(name),
          m_Handle{},
          m_Width(width),
          m_Height(height),
          m_ChannelCount(channelCount),
          m_HasTransparency(hasTransparency)
    {
    }

    void Texture2D::LoadFromFile(const std::string& name)
    {
        int32_t width, height, channels;
        auto data = LoadRawData(name, width, height, channels, m_HasTransparency);

        m_Width = static_cast<uint32_t>(width);
        m_Height = static_cast<uint32_t>(height);
        m_ChannelCount = static_cast<uint8_t>(channels);

        switch (Renderer::RenderCommand::GetApiType())
        {
            case Platform::RHI_API_TYPE::Vulkan:
            {
                UpdateInternalData(Renderer::RenderCommand::GetRHIRef(), data);
                return;
            }
            default:
                break;
        }

        VA_ENGINE_WARN("[RenderCommand] Failed to create a texture {}.", name);
    }

    std::vector<uint8_t> Texture2D::LoadRawData(
        const std::string& name,
        int32_t& width,
        int32_t& height,
        int32_t& channels,
        bool& hasTransparency)
    {
        std::stringstream ss;
        ss << "assets/textures/" << name << ".png";
        //TODO Try other formats like JPG, TGA, BMP, etc.

        const auto rawData = stbi_load(ss.str().c_str(), &width, &height, &channels, 4);
        if (rawData == nullptr)
        {
            VA_ENGINE_WARN(
                "[RenderCommand] Failed to load texture '{}', with error {}.",
                name,
                stbi_failure_reason());
            return {};
        }
        auto data = std::vector<uint8_t>(width * height * 4);
        memcpy(data.data(), rawData, data.size());
        stbi_image_free(rawData);

        // Search for transparency
        hasTransparency = false;
        for (size_t i = 0; i < data.size(); i += 4)
        {
            if (data[i + 3] < 255)
            {
                hasTransparency = true;
                break;
            }
        }

        return data;
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
} // VoidArchitect
