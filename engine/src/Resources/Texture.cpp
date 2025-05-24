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
        auto texture = std::shared_ptr<Texture2D>(nullptr);
        Renderer::RenderCommand::CreateTexture2D(name);
        return texture;
    }

    void Texture2D::LoadFromFile(const std::string& name)
    {
        std::stringstream ss;
        ss << "assets/textures/" << name << ".png";
        //TODO Try other formats like JPG, TGA, BMP, etc.

        int32_t width, height, channels;
        const auto rawData = stbi_load(ss.str().c_str(), &width, &height, &channels, 4);
        if (rawData == nullptr)
        {
            VA_ENGINE_WARN(
                "[RenderCommand] Failed to load texture '{}', with error {}.",
                name,
                stbi_failure_reason());
            return;
        }
        auto data = std::vector<uint8_t>(width * height * 4);
        memcpy(data.data(), rawData, data.size());
        stbi_image_free(rawData);

        // Search for transparency
        m_HasTransparency = false;
        for (size_t i = 0; i < data.size(); i += 4)
        {
            if (data[i + 3] < 255)
            {
                m_HasTransparency = true;
                break;
            }
        }

        switch (Renderer::RenderCommand::GetApiType())
        {
            case Platform::RHI_API_TYPE::Vulkan:
            {
                m_Generation++;
                UpdateInternalData(Renderer::RenderCommand::GetRHIRef(), data);
                return;
            }
            default:
                break;
        }

        VA_ENGINE_WARN("[RenderCommand] Failed to create a texture {}.", name);
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
