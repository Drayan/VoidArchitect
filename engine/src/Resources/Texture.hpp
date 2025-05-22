//
// Created by Michael Desmedt on 22/05/2025.
//
#pragma once

namespace VoidArchitect::Resources
{
    class ITexture
    {
    public:
        ITexture() = default;

    protected:
        uint32_t m_Handle;

        uint32_t m_Width, m_Height;
        uint8_t m_ChannelCount;
        bool m_HasTransparency;

        uint32_t m_Generation;
    };

    class Texture2D : public ITexture
    {
    public:
        static std::shared_ptr<Texture2D> Create(const std::string& name);
    };
}
