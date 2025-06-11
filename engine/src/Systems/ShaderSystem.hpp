//
// Created by Michael Desmedt on 27/05/2025.
//
#pragma once
#include "RenderStateSystem.hpp"
#include "Core/Uuid.hpp"
#include "Resources/Shader.hpp"

namespace VoidArchitect
{
    struct ShaderConfig
    {
        Resources::ShaderStage stage;
        std::string entry;
    };

    class ShaderSystem
    {
    public:
        ShaderSystem();
        ~ShaderSystem() = default;

        Resources::ShaderHandle GetHandleFor(const std::string& name);
        Resources::IShader* GetPointerFor(Resources::ShaderHandle shaderHandle);

    private:
        Resources::IShader* LoadShader(const std::string& name);
        void LoadDefaultShaders();
        void ReleaseShader(const Resources::IShader* shader);
        uint32_t GetFreeShaderHandle();

        std::queue<uint32_t> m_FreeShaderHandles;
        uint32_t m_NextFreeShaderHandle = 0;

        VAArray<std::unique_ptr<Resources::IShader>> m_Shaders;
    };

    inline std::unique_ptr<ShaderSystem> g_ShaderSystem;
} // namespace VoidArchitect
