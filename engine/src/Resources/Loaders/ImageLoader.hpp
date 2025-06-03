//
// Created by Michael Desmedt on 01/06/2025.
//
#pragma once
#include "Loader.hpp"
#include "ResourceDefinition.hpp"

namespace VoidArchitect::Resources::Loaders
{
    class ImageLoader;

    class ImageDataDefinition final : public IResourceDefinition
    {
        friend class ImageLoader;

    public:
        ~ImageDataDefinition() override = default;

        const std::vector<uint8_t>& GetData() const { return m_Data; }
        int GetWidth() const { return m_Width; }
        int GetHeight() const { return m_Height; }
        int GetBPP() const { return m_BPP; }
        bool HasTransparency() const { return m_HasTransparency; }

    private:
        ImageDataDefinition(
            std::vector<uint8_t> data,
            int32_t width,
            int32_t height,
            int32_t bpp,
            bool hasTransparency);

        std::vector<uint8_t> m_Data;
        int m_Width, m_Height, m_BPP;
        bool m_HasTransparency;
    };

    using ImageDataDefinitionPtr = std::shared_ptr<ImageDataDefinition>;

    class ImageLoader final : public ILoader
    {
    public:
        ImageLoader(const std::string& baseAssetPath);
        ~ImageLoader() override = default;

        std::shared_ptr<IResourceDefinition> Load(const std::string& name) override;
    };
} // namespace VoidArchitect::Resources::Loaders
