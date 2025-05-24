//
// Created by Michael Desmedt on 22/05/2025.
//
#pragma once

namespace VoidArchitect::Resources
{
    class ITexture
    {
    public:
        ITexture(uint32_t width, uint32_t height, uint8_t channelCount, bool hasTransparency);
        virtual ~ITexture() = default;

        uint32_t GetGeneration() const { return m_Generation; }
        void SetGeneration(const uint32_t generation) { m_Generation = generation; }

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

    protected:
        Texture2D(uint32_t width, uint32_t height, uint8_t channelCount, bool hasTransparency);
    };
}
