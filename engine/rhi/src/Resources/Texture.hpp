//
// Created by Michael Desmedt on 22/05/2025.
//
#pragma once
#include <VoidArchitect/Engine/Common/Handle.hpp>
#include <VoidArchitect/Engine/Common/Uuid.hpp>

namespace VoidArchitect
{
    class TextureSystem;
    struct TextureNode;
}

namespace VoidArchitect::Platform
{
    class IRenderingHardware;
}

namespace VoidArchitect::Resources
{
    enum class TextureUse
    {
        None = 0,
        Diffuse = 1 << 0,
        Normal = 1 << 1,
        Specular = 1 << 2,
        Emissive = 1 << 3,
        Metallic = 1 << 4,
        Roughness = 1 << 5,
        AmbientOcclusion = 1 << 6,
        Depth = 1 << 7
    };

    enum class TextureFilterMode
    {
        Nearest,
        Linear,
    };

    enum class TextureRepeatMode
    {
        Repeat,
        ClampToEdge,
        ClampToBorder,
        MirroredRepeat,
    };

    using TextureHandle = Handle<TextureNode>;
    static constexpr TextureHandle InvalidTextureHandle = TextureHandle::Invalid();

    class ITexture
    {
        friend class VoidArchitect::TextureSystem;

    public:
        ITexture(
            std::string name,
            uint32_t width,
            uint32_t height,
            uint8_t channelCount,
            bool hasTransparency);
        virtual ~ITexture() = default;

        UUID GetUUID() const { return m_UUID; }

    protected:
        virtual void Release() = 0;

        std::string m_Name;
        UUID m_UUID;

        uint32_t m_Width, m_Height;
        uint8_t m_ChannelCount;
        bool m_HasTransparency;
        bool m_IsWritable = false;

        TextureFilterMode m_FilterModeMin = TextureFilterMode::Linear;
        TextureFilterMode m_FilterModeMag = TextureFilterMode::Linear;

        TextureRepeatMode m_RepeatModeU = TextureRepeatMode::Repeat;
        TextureRepeatMode m_RepeatModeV = TextureRepeatMode::Repeat;
        TextureRepeatMode m_RepeatModeW = TextureRepeatMode::Repeat;

        TextureUse m_Use = TextureUse::None;
    };

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
} // namespace VoidArchitect::Resources
