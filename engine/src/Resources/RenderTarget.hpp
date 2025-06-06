//
// Created by Michael Desmedt on 02/06/2025.
//
#pragma once

#include "Core/Uuid.hpp"

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
        class IRenderTarget
        {
            friend class VoidArchitect::Renderer::RenderGraph;

        public:
            virtual ~IRenderTarget() = default;

            [[nodiscard]] UUID GetUUID() const { return m_UUID; }
            [[nodiscard]] const std::string& GetName() const { return m_Name; }
            [[nodiscard]] uint32_t GetWidth() const { return m_Width; }
            [[nodiscard]] uint32_t GetHeight() const { return m_Height; }
            [[nodiscard]] bool IsMainTarget() const { return m_IsMain; }

            // Resize support (for main target or dynamic targets)
            virtual void Resize(uint32_t width, uint32_t height) = 0;

        protected:
            IRenderTarget(
                std::string name,
                uint32_t width,
                uint32_t height,
                Renderer::TextureFormat format,
                bool isMain = false);

            virtual void Release() = 0;

            UUID m_UUID;
            std::string m_Name;
            uint32_t m_Width;
            uint32_t m_Height;
            bool m_IsMain;
            Renderer::TextureFormat m_Format;
        };

        using RenderTargetPtr = std::shared_ptr<IRenderTarget>;
    } // namespace Resources
} // namespace VoidArchitect
