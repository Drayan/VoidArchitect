//
// Created by Michael Desmedt on 01/06/2025.
//

#include "ImageLoader.hpp"

#include <stb_image.h>

#include "Core/Logger.hpp"

namespace VoidArchitect::Resources::Loaders
{
    ImageDataDefinition::ImageDataDefinition(
        VAArray<uint8_t> data,
        const int32_t width,
        const int32_t height,
        const int32_t bpp,
        const bool hasTransparency)
        : IResourceDefinition(),
          m_Data(std::move(data)),
          m_Width(width),
          m_Height(height),
          m_BPP(bpp),
          m_HasTransparency(hasTransparency)
    {
    }

    ImageLoader::ImageLoader(const std::string& baseAssetPath) : ILoader(baseAssetPath) {}

    std::shared_ptr<IResourceDefinition> ImageLoader::Load(const std::string& name)
    {
        std::stringstream ss;
        ss << m_BaseAssetPath << name << ".png";
        // TODO Try other formats like JPG, TGA, BMP, etc.

        int32_t width, height, channels;
        const auto rawData = stbi_load(ss.str().c_str(), &width, &height, &channels, 4);
        if (rawData == nullptr)
        {
            VA_ENGINE_WARN(
                "[RenderCommand] Failed to load texture '{}', with error {}.",
                name,
                stbi_failure_reason());
            return {};
        }
        auto data = VAArray<uint8_t>(width * height * 4);
        memcpy(data.data(), rawData, data.size());
        stbi_image_free(rawData);

        // Search for transparency
        auto hasTransparency = false;
        for (size_t i = 0; i < data.size(); i += 4)
        {
            if (data[i + 3] < 255)
            {
                hasTransparency = true;
                break;
            }
        }

        auto imageDefinition =
            new ImageDataDefinition(std::move(data), width, height, 4, hasTransparency);
        return ImageDataDefinitionPtr(imageDefinition);
    }
} // namespace VoidArchitect::Resources::Loaders
