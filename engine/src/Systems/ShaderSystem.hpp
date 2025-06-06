//
// Created by Michael Desmedt on 27/05/2025.
//
#pragma once
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

        Resources::ShaderPtr LoadShader(const std::string& name);

    private:
        void LoadDefaultShaders();
        void ReleaseShader(const Resources::IShader* shader);
        uint32_t GetFreeShaderHandle();

        struct ShaderData
        {
            UUID uuid;
        };

        struct ShaderDeleter
        {
            ShaderSystem* system;
            void operator()(const Resources::IShader* shader) const;
        };

        std::queue<uint32_t> m_FreeShaderHandles;
        uint32_t m_NextShaderHandle = 0;

        VAArray<ShaderData> m_Shaders;
        VAHashMap<UUID, std::weak_ptr<Resources::IShader>> m_ShaderCache;
    };

    inline std::unique_ptr<ShaderSystem> g_ShaderSystem;
} // namespace VoidArchitect
