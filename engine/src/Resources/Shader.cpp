//
// Created by Michael Desmedt on 27/05/2025.
//
#include "Shader.hpp"

#include "Core/Logger.hpp"

namespace VoidArchitect::Resources
{
    ShaderStage ShaderStageFromString(const std::string& stage)
    {
        if (stage == "vertex")
            return ShaderStage::Vertex;
        if (stage == "pixel" || stage == "fragment" || stage == "frag")
            return ShaderStage::Pixel;
        if (stage == "compute")
            return ShaderStage::Compute;
        if (stage == "geometry")
            return ShaderStage::Geometry;
        if (stage == "tessellation_control")
            return ShaderStage::TessellationControl;
        if (stage == "tessellation_evaluation")
            return ShaderStage::TessellationEvaluation;
        if (stage == "all")
            return ShaderStage::All;

        VA_ENGINE_WARN("[Shader] Unknown shader stage '{}', defaulting to Pixel.", stage);
        return ShaderStage::Pixel;
    }

    IShader::IShader(const std::string& name, ShaderStage stage, const std::string& entryPoint)
        : m_Name(name),
          m_Stage(stage),
          m_EntryPoint(entryPoint)
    {
    }
} // namespace VoidArchitect::Resources
