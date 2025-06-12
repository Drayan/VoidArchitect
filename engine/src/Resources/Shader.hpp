//
// Created by Michael Desmedt on 27/05/2025.
//
#pragma once
#include "Core/Uuid.hpp"

namespace VoidArchitect
{
    class ShaderSystem;

    namespace Resources
    {
        enum class ShaderStage
        {
            Vertex,
            Pixel,
            Compute,
            Geometry,
            TessellationControl,
            TessellationEvaluation,
            All
        };

        ShaderStage ShaderStageFromString(const std::string& stage);

        using ShaderHandle = uint32_t;
        static constexpr ShaderHandle InvalidShaderHandle = std::numeric_limits<
            ShaderHandle>::max();

        class IShader
        {
            friend class VoidArchitect::ShaderSystem;

        public:
            virtual ~IShader() = default;

        protected:
            IShader() = default;
            IShader(const std::string& name, ShaderStage stage, const std::string& entryPoint);

            std::string m_Name;
            UUID m_UUID;

            ShaderStage m_Stage;
            std::string m_EntryPoint;
        };
    } // namespace Resources
} // namespace VoidArchitect
