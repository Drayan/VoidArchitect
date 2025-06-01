//
// Created by Michael Desmedt on 01/06/2025.
//
#include "MaterialLoader.hpp"

#include "Core/Logger.hpp"

#include <yaml-cpp/yaml.h>

#include "Systems/PipelineSystem.hpp"

namespace VoidArchitect::Resources::Loaders
{
    MaterialDataDefinition::MaterialDataDefinition(const MaterialConfig& config)
        : m_MaterialConfig(config)
    {
    }

    MaterialLoader::MaterialLoader(const std::string& baseAssetPath)
        : ILoader(baseAssetPath)
    {
    }

    std::shared_ptr<IResourceDefinition> MaterialLoader::Load(const std::string& name)
    {
        // Load material logic here, it should retrieve the config file for this material,
        // parse it, and then create the material using the CreateMaterial method.
        const std::string materialPath = m_BaseAssetPath + name + ".yaml";

        // Check that the YAML file exists
        if (!std::filesystem::exists(materialPath))
        {
            VA_ENGINE_WARN("[MaterialSystem] Material file '{}' does not exist.", materialPath);
            return nullptr;
        }

        try
        {
            auto configFile = YAML::LoadFile(materialPath);
            MaterialConfig config;

            if (configFile["material"])
            {
                auto materialNode = configFile["material"];

                // Name (mandatory)
                if (materialNode["name"])
                {
                    auto name = materialNode["name"].as<std::string>();
                    config.name = name;
                }
                else
                {
                    VA_ENGINE_ERROR("[MaterialSystem] Material file '{}' is missing name.", name);
                    return nullptr;
                }

                // Pipeline (optional)
                if (materialNode["pipeline"])
                {
                    auto pipeline = materialNode["pipeline"].as<std::string>();
                    // TODO Retrieve the pipeline from the pipeline system
                    // config.pipeline = pipeline;
                }
                else
                {
                    config.pipeline = g_PipelineSystem->GetDefaultPipeline();
                }

                // Properties (required)
                if (materialNode["properties"])
                {
                    auto propertiesNode = materialNode["properties"];

                    // Diffuse color (optional)
                    if (propertiesNode["diffuse_color"])
                    {
                        if (propertiesNode["diffuse_color"].size() != 4)
                        {
                            VA_ENGINE_ERROR(
                                "[MaterialSystem] Material file '{}' has invalid diffuse color, "
                                "defaulting to white.",
                                name);
                            config.diffuseColor = Math::Vec4::One();
                        }
                        else
                        {
                            std::vector<float> values;
                            for (auto val : propertiesNode["diffuse_color"])
                                values.push_back(val.as<float>());
                            config.diffuseColor =
                                Math::Vec4(values[0], values[1], values[2], values[3]);
                        }
                    }
                    else
                    {
                        config.diffuseColor = Math::Vec4::One();
                    }

                    // Diffuse map (optional)
                    if (propertiesNode["diffuse_map"])
                    {
                        auto diffuseMapName = propertiesNode["diffuse_map"].as<std::string>();
                        config.diffuseTexture.name = diffuseMapName;
                        config.diffuseTexture.use = Resources::TextureUse::Diffuse;
                    }
                    else
                    {
                        config.diffuseTexture.name = "";
                    }

                    // TODO Parse other properties
                }
                else
                {
                    VA_ENGINE_ERROR(
                        "[MaterialSystem] Material file '{}' is missing properties.",
                        name);
                    return nullptr;
                }
            }

            // At this point, we should have a valid material config.
            auto materialData = new MaterialDataDefinition(config);
            return MaterialDataDefinitionPtr(materialData);
        }
        catch (YAML::Exception& ex)
        {
            VA_ENGINE_ERROR(
                "[MaterialSystem] Failed to parse material file '{}': {}",
                materialPath,
                ex.what());
        }
        return nullptr;
    }
}
