//
// Created by Michael Desmedt on 02/06/2025.
//
#include "RenderTarget.hpp"

#include <utility>

namespace VoidArchitect::Resources
{
    IRenderTarget::IRenderTarget(
        std::string name,
        const uint32_t width,
        const uint32_t height,
        const Renderer::TextureFormat format,
        const bool isMain)
        : m_Name(std::move(name)),
          m_Width(width),
          m_Height(height),
          m_IsMain(isMain),
          m_Format(format)
    {
    }
}
