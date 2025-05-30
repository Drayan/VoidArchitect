//
// Created by Michael Desmedt on 27/05/2025.
//
#include "Shader.hpp"

namespace VoidArchitect::Resources
{
    IShader::IShader(const std::string& name, ShaderStage stage, const std::string& entryPoint)
        : m_Name(name),
          m_Stage(stage),
          m_EntryPoint(entryPoint)
    {
    }

} // namespace VoidArchitect::Resources
