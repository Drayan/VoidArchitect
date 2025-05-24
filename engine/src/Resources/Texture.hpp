//
// Created by Michael Desmedt on 22/05/2025.
//
#pragma once

namespace VoidArchitect::Platform
{
    class IRenderingHardware;
}

namespace VoidArchitect::Resources
{
    class ITexture
    {
    public:
        ITexture(uint32_t width, uint32_t height, uint8_t channelCount, bool hasTransparency);
        virtual ~ITexture() = default;

    protected:
        uint32_t m_Handle;

        uint32_t m_Width, m_Height;
        uint8_t m_ChannelCount;
        bool m_HasTransparency;
    };

    class Texture2D : public ITexture
    {
    public:
        static std::shared_ptr<Texture2D> Create(const std::string& name);
        static std::shared_ptr<Texture2D> Create(
            uint32_t width,
            uint32_t height,
            uint8_t channels,
            bool hasTransparency,
            const std::vector<uint8_t>& data);

        void LoadFromFile(const std::string& name);

    protected:
        Texture2D(uint32_t width, uint32_t height, uint8_t channelCount, bool hasTransparency);

        virtual void UpdateInternalData(
            Platform::IRenderingHardware& rhi,
            std::vector<uint8_t>& data) = 0;
    };
}
