//
// Created by Michael Desmedt on 27/05/2025.
//
#include "ShaderSystem.hpp"

#include "Core/Logger.hpp"
#include "Platform/RHI/IRenderingHardware.hpp"
#include "Renderer/RenderCommand.hpp"
#include "ResourceSystem.hpp"
#include "Resources/Loaders/ShaderLoader.hpp"

namespace VoidArchitect
{
    ShaderSystem::ShaderSystem() { m_Shaders.reserve(128); }

    Resources::ShaderPtr ShaderSystem::LoadShader(const std::string& name)
    {
        // 1. Check if the requested shader is in the cache
        for (auto& [uuid, shader] : m_ShaderCache)
        {
            const auto& shd = shader.lock();
            if (shd && shd->m_Name == name)
            {
                return shd;
            }

            if (shd == nullptr)
            {
                m_ShaderCache.erase(uuid);
            }
        }

        // Load the shader with the resource loader
        const auto shaderData =
            g_ResourceSystem->LoadResource<Resources::Loaders::ShaderDataDefinition>(
                ResourceType::Shader, name);

        // 5. Create a new shader resource
        Resources::IShader* shader = nullptr;
        switch (Renderer::RenderCommand::GetApiType())
        {
            case Platform::RHI_API_TYPE::Vulkan:
            {
                shader = Renderer::RenderCommand::GetRHIRef().CreateShader(
                    name, shaderData->GetConfig(), shaderData->GetCode());
            }
            default:
                break;
        }

        if (!shader)
        {
            VA_ENGINE_WARN("[ShaderSystem] Failed to create shader '{}'.", name);
            return nullptr;
        }

        // 6. Store the shader in the cache
        auto shaderPtr = Resources::ShaderPtr(shader, ShaderDeleter{this});
        m_ShaderCache[shader->m_UUID] = shaderPtr;

        VA_ENGINE_TRACE("[ShaderSystem] Shader '{}' loaded.", name);

        return shaderPtr;
    }

    void ShaderSystem::ReleaseShader(const Resources::IShader* shader)
    {
        if (const auto it = m_ShaderCache.find(shader->m_UUID); it != m_ShaderCache.end())
        {
            VA_ENGINE_TRACE("[ShaderSystem] Releasing shader '{}'.", shader->m_Name);

            m_ShaderCache.erase(it);
        }
    }

    void ShaderSystem::ShaderDeleter::operator()(const Resources::IShader* shader) const
    {
        system->ReleaseShader(shader);
        delete shader;
    }
} // namespace VoidArchitect
