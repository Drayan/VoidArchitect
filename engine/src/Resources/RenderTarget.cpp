//
// Created by Michael Desmedt on 02/06/2025.
//
#include "RenderTarget.hpp"

namespace VoidArchitect::Resources
{
    IRenderTarget::IRenderTarget(
        const std::string& name,
        const uint32_t width,
        const uint32_t height,
        const bool isMain)
        : m_Name(name),
          m_Width(width),
          m_Height(height),
          m_IsMain(isMain)
    {
    }
}
