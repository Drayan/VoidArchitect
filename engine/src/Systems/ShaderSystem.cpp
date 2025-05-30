//
// Created by Michael Desmedt on 27/05/2025.
//
#include "ShaderSystem.hpp"

#include "Core/Logger.hpp"
#include "Platform/RHI/IRenderingHardware.hpp"
#include "Renderer/RenderCommand.hpp"

#include <regex>
#include <yaml-cpp/yaml.h>

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

        // 2. Load the shader from the file
        // TODO Use a resource loader
        const std::string shaderPath = DefaultShaderPath + name + DefaultShaderExtension;
        std::ifstream shaderFile(shaderPath, std::ios::ate | std::ios::binary);

        if (!shaderFile.is_open())
        {
            VA_ENGINE_WARN("[ShaderSystem] Failed to load shader {}, at path {}", name, shaderPath);
            return nullptr;
        }

        // 3. Read the shader file into a buffer
        const std::streamsize fileSize = shaderFile.tellg();
        std::vector<uint8_t> buffer(fileSize);
        shaderFile.seekg(0);
        shaderFile.read(reinterpret_cast<std::istream::char_type*>(buffer.data()), fileSize);
        shaderFile.close();

        // 4. Parse shader metadata
        auto config = ParseShaderMetadata(shaderPath);

        // 5. Create a new shader resource
        Resources::IShader* shader = nullptr;
        switch (Renderer::RenderCommand::GetApiType())
        {
            case Platform::RHI_API_TYPE::Vulkan:
            {
                shader = Renderer::RenderCommand::GetRHIRef().CreateShader(name, config, buffer);
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

    ShaderConfig ShaderSystem::ParseShaderMetadata(const std::string& path)
    {
        auto yamlPath = path;

        // Replace .shader extension with .yaml
        if (yamlPath.find(".shader") != std::string::npos)
        {
            yamlPath.replace(yamlPath.find(".shader"), 7, ".yaml");
        }

        // Check that the YAML file exists
        if (!std::filesystem::exists(yamlPath))
        {
            VA_ENGINE_WARN("[ShaderSystem] Shader metadata file '{}' does not exist.", yamlPath);
            return InferMetadataFromFilename(path);
        }

        // Load and parse the YAML file
        try
        {
            auto config = YAML::LoadFile(yamlPath);
            ShaderConfig metadata;

            if (config["shader"])
            {
                auto shaderNode = config["shader"];

                // Stage (mandatory)
                if (shaderNode["stage"])
                {
                    auto stage = shaderNode["stage"].as<std::string>();
                    metadata.stage = StringToShaderStage(stage);
                }
                else
                {
                    VA_ENGINE_WARN(
                        "[ShaderSystem] Shader metadata file '{}' is missing stage.", yamlPath);
                    return InferMetadataFromFilename(path);
                }

                // Entry point (optional)
                if (shaderNode["entry"])
                {
                    auto entryPoint = shaderNode["entry"].as<std::string>();
                    metadata.entry = entryPoint;
                }
                else
                {
                    metadata.entry = "main";
                }
            }
            else
            {
                VA_ENGINE_WARN(
                    "[ShaderSystem] Missing 'shader' section in YAML header of shader {}", path);
                return InferMetadataFromFilename(path);
            }

            VA_ENGINE_TRACE("[ShaderSystem] Shader metadata loaded from file '{}'.", yamlPath);
            return metadata;
        }
        catch (YAML::Exception& ex)
        {
            VA_ENGINE_WARN(
                "[ShaderSystem] Failed to parse shader metadata file '{}': {}",
                yamlPath,
                ex.what());
            return InferMetadataFromFilename(path);
        }
        catch (const std::exception& ex)
        {
            VA_ENGINE_WARN(
                "[ShaderSystem] Failed to parse shader metadata file '{}': {}",
                yamlPath,
                ex.what());
            return InferMetadataFromFilename(path);
        }
    }

    ShaderConfig ShaderSystem::InferMetadataFromFilename(const std::string& name)
    {
        if (name.find("vert") != std::string::npos)
        {
            return {Resources::ShaderStage::Vertex, "main"};
        }
        if (name.find("frag") != std::string::npos || name.find("pixl") != std::string::npos)
        {
            return {Resources::ShaderStage::Pixel, "main"};
        }

        VA_ENGINE_WARN(
            "[ShaderSystem] Failed to determine shader stage for '{}', defaulting to Pixel.", name);
        return {Resources::ShaderStage::Pixel, "main"};
    }

    Resources::ShaderStage ShaderSystem::StringToShaderStage(const std::string& stage)
    {
        if (stage == "vertex")
            return Resources::ShaderStage::Vertex;
        if (stage == "pixel" || stage == "fragment")
            return Resources::ShaderStage::Pixel;
        if (stage == "compute")
            return Resources::ShaderStage::Compute;
        if (stage == "geometry")
            return Resources::ShaderStage::Geometry;

        VA_ENGINE_WARN("[ShaderSystem] Unknown shader stage '{}', defaulting to Pixel.", stage);
        return Resources::ShaderStage::Pixel;
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
