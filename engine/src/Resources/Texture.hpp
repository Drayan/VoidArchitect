//
// Created by Michael Desmedt on 22/05/2025.
//
#pragma once
#include "Core/Uuid.hpp"

namespace VoidArchitect
{
    class TextureSystem;
}

namespace VoidArchitect::Platform
{
    class IRenderingHardware;
}

namespace VoidArchitect::Resources
{
    class ITexture
    {
        friend class VoidArchitect::TextureSystem;

    public:
        ITexture(
            const std::string& name,
            uint32_t width,
            uint32_t height,
            uint8_t channelCount,
            bool hasTransparency);
        virtual ~ITexture() = default;

        UUID GetUUID() const { return m_UUID; }

    protected:
        virtual void Release() = 0;

        std::string m_Name;
        uint32_t m_Handle;
        UUID m_UUID;

        uint32_t m_Width, m_Height;
        uint8_t m_ChannelCount;
        bool m_HasTransparency;
    };

    using TexturePtr = std::shared_ptr<ITexture>;

    class Texture2D : public ITexture
    {
    protected:
        Texture2D(
            const std::string& name,
            uint32_t width,
            uint32_t height,
            uint8_t channelCount,
            bool hasTransparency);
    };

    using Texture2DPtr = std::shared_ptr<Texture2D>;
}
