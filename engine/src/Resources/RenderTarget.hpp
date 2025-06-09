//
// Created by Michael Desmedt on 02/06/2025.
//
#pragma once
namespace VoidArchitect
{
    namespace Renderer
    {
        enum class TextureFormat;
        class RenderGraph;
    }

    namespace Platform
    {
        class IRenderingHardware;
    }

    namespace Resources
    {
        using RenderTargetHandle = uint32_t;
        static constexpr RenderTargetHandle InvalidRenderTargetHandle = std::numeric_limits<
            uint32_t>::max();

        class IRenderTarget
        {
            friend class VoidArchitect::Renderer::RenderGraph;

        public:
            virtual ~IRenderTarget() = default;

            [[nodiscard]] const std::string& GetName() const { return m_Name; }
            [[nodiscard]] uint32_t GetWidth() const { return m_Width; }
            [[nodiscard]] uint32_t GetHeight() const { return m_Height; }
            [[nodiscard]] Renderer::TextureFormat GetFormat() const { return m_Format; }

        protected:
            IRenderTarget(
                std::string name,
                uint32_t width,
                uint32_t height,
                Renderer::TextureFormat format);

            std::string m_Name;
            uint32_t m_Width;
            uint32_t m_Height;
            Renderer::TextureFormat m_Format;
        };
    } // namespace Resources
} // namespace VoidArchitect
