//
// Created by Michael Desmedt on 27/05/2025.
//
#include "ShaderSystem.hpp"

#include "Core/Logger.hpp"
#include "Platform/RHI/IRenderingHardware.hpp"
#include "ResourceSystem.hpp"
#include "Renderer/RenderSystem.hpp"
#include "Resources/Loaders/ShaderLoader.hpp"

namespace VoidArchitect
{
    ShaderSystem::ShaderSystem() { m_Shaders.reserve(128); }

    Resources::ShaderHandle ShaderSystem::GetHandleFor(const std::string& name)
    {
        // Check if the shader is already loaded
        for (uint32_t i = 0; i < m_Shaders.size(); i++)
        {
            if (m_Shaders[i]->m_Name == name) return i;
        }

        // This is the first time the system is asked to load this shader;
        // we have to load it from disk.
        const auto shaderPtr = LoadShader(name);
        const auto handle = GetFreeShaderHandle();
        m_Shaders[handle] = std::unique_ptr<Resources::IShader>(shaderPtr);

        return handle;
    }

    Resources::IShader* ShaderSystem::GetPointerFor(Resources::ShaderHandle shaderHandle)
    {
        return m_Shaders[shaderHandle].get();
    }

    Resources::IShader* ShaderSystem::LoadShader(const std::string& name)
    {
        // Load the shader with the resource loader
        const auto shaderData = g_ResourceSystem->LoadResource<
            Resources::Loaders::ShaderDataDefinition>(ResourceType::Shader, name);

        // 5. Create a new shader resource
        Resources::IShader* shader = Renderer::g_RenderSystem->GetRHI()->CreateShader(
            name,
            shaderData->GetConfig(),
            shaderData->GetCode());
        if (!shader)
        {
            VA_ENGINE_WARN("[ShaderSystem] Failed to create shader '{}'.", name);
            return nullptr;
        }

        VA_ENGINE_TRACE("[ShaderSystem] Shader '{}' loaded.", name);

        return shader;
    }

    void ShaderSystem::ReleaseShader(const Resources::IShader* shader)
    {
    }

    uint32_t ShaderSystem::GetFreeShaderHandle()
    {
        // If there are free handles, return the first one
        if (!m_FreeShaderHandles.empty())
        {
            const uint32_t handle = m_FreeShaderHandles.front();
            m_FreeShaderHandles.pop();
            return handle;
        }

        // Otherwise, return the next handle and increment it
        if (m_NextFreeShaderHandle >= m_Shaders.size())
        {
            m_Shaders.resize(m_NextFreeShaderHandle + 1);
        }
        return m_NextFreeShaderHandle++;
    }
} // namespace VoidArchitect
