//
// Created by Michael Desmedt on 27/05/2025.
//
#include "RenderState.hpp"

namespace VoidArchitect::Resources
{
    IRenderState::IRenderState(const std::string& name, const RenderStateInputLayout& inputLayout)
        : m_Name(name), m_InputLayout(inputLayout)
    {
    }
} // namespace VoidArchitect::Resources
