//
// Created by Michael Desmedt on 01/06/2025.
//
#include "ShaderLoader.hpp"

#include "Core/Logger.hpp"

#include <yaml-cpp/yaml.h>

namespace VoidArchitect::Resources::Loaders
{
    ShaderDataDefinition::ShaderDataDefinition(
        const ShaderConfig& config, VAArray<uint8_t> code)
        : m_ShaderConfig(config),
          m_Code(std::move(code))
    {
    }

    ShaderLoader::ShaderLoader(const std::string& baseAssetPath) : ILoader(baseAssetPath) {}

    std::shared_ptr<IResourceDefinition> ShaderLoader::Load(const std::string& name)
    {
        const std::string shaderPath = m_BaseAssetPath + name + ".shader";
        std::ifstream shaderFile(shaderPath, std::ios::ate | std::ios::binary);

        if (!shaderFile.is_open())
        {
            VA_ENGINE_WARN("[ShaderLoader] Failed to load shader {}, at path {}", name, shaderPath);
            return nullptr;
        }

        // Read the shader file into a buffer
        const std::streamsize fileSize = shaderFile.tellg();
        VAArray<uint8_t> buffer(fileSize);
        shaderFile.seekg(0);
        shaderFile.read(reinterpret_cast<std::istream::char_type*>(buffer.data()), fileSize);
        shaderFile.close();

        // Parse shader metadata
        const auto config = ParseShaderMetadata(shaderPath);
        const auto shaderData = new ShaderDataDefinition(config, std::move(buffer));

        return ShaderDataDefinitionPtr(shaderData);
    }

    ShaderConfig ShaderLoader::ParseShaderMetadata(const std::string& path)
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
            VA_ENGINE_WARN("[ShaderLoader] Shader metadata file '{}' does not exist.", yamlPath);
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
                        "[ShaderLoader] Shader metadata file '{}' is missing stage.", yamlPath);
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
                    "[ShaderLoader] Missing 'shader' section in YAML header of shader {}", path);
                return InferMetadataFromFilename(path);
            }

            VA_ENGINE_TRACE("[ShaderLoader] Shader metadata loaded from file '{}'.", yamlPath);
            return metadata;
        }
        catch (YAML::Exception& ex)
        {
            VA_ENGINE_WARN(
                "[ShaderLoader] Failed to parse shader metadata file '{}': {}",
                yamlPath,
                ex.what());
            return InferMetadataFromFilename(path);
        }
        catch (const std::exception& ex)
        {
            VA_ENGINE_WARN(
                "[ShaderLoader] Failed to parse shader metadata file '{}': {}",
                yamlPath,
                ex.what());
            return InferMetadataFromFilename(path);
        }
    }

    ShaderConfig ShaderLoader::InferMetadataFromFilename(const std::string& name)
    {
        if (name.find("vert") != std::string::npos)
        {
            return {ShaderStage::Vertex, "main"};
        }
        if (name.find("frag") != std::string::npos || name.find("pixl") != std::string::npos)
        {
            return {ShaderStage::Pixel, "main"};
        }

        VA_ENGINE_WARN(
            "[ShaderLoader] Failed to determine shader stage for '{}', defaulting to Pixel.", name);
        return {ShaderStage::Pixel, "main"};
    }

    ShaderStage ShaderLoader::StringToShaderStage(const std::string& stage)
    {
        if (stage == "vertex")
            return Resources::ShaderStage::Vertex;
        if (stage == "pixel" || stage == "fragment")
            return Resources::ShaderStage::Pixel;
        if (stage == "compute")
            return Resources::ShaderStage::Compute;
        if (stage == "geometry")
            return Resources::ShaderStage::Geometry;

        VA_ENGINE_WARN("[ShaderLoader] Unknown shader stage '{}', defaulting to Pixel.", stage);
        return Resources::ShaderStage::Pixel;
    }
} // namespace VoidArchitect::Resources::Loaders
