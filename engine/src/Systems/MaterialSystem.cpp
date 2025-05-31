#include "MaterialSystem.hpp"

#include "Core/Logger.hpp"
#include "PipelineSystem.hpp"
#include "Platform/RHI/IRenderingHardware.hpp"
#include "Renderer/RenderCommand.hpp"
#include "Resources/Material.hpp"
#include "TextureSystem.hpp"

#include <yaml-cpp/yaml.h>

namespace VoidArchitect
{
    MaterialSystem::MaterialSystem()
    {
        m_Materials.reserve(
            1024); // Reserve space for 1024 materials to avoid frequent reallocations
        GenerateDefaultMaterials();
    }

    MaterialSystem::~MaterialSystem() {}

    Resources::MaterialPtr MaterialSystem::LoadMaterial(const std::string& name)
    {
        // Check if the material is already loaded in the cache
        for (auto& [uuid, material] : m_MaterialCache)
        {
            const auto& mat = material.lock();
            if (mat && mat->m_Name == name)
            {
                // If the material is found in the cache, return it
                return mat;
            }

            if (mat == nullptr)
            {
                // Remove expired weak pointers from the cache
                m_MaterialCache.erase(uuid);
            }
        }

        // Load material logic here, it should retrieve the config file for this material,
        // parse it, and then create the material using the CreateMaterial method.
        const std::string DefaultMaterialPath{"../../../../../assets/materials/"};
        const std::string materialPath = DefaultMaterialPath + name + ".yaml";

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
                        "[MaterialSystem] Material file '{}' is missing properties.", name);
                    return nullptr;
                }
            }

            // At this point, we should have a valid material config.
            return CreateMaterial(config);
        }
        catch (YAML::Exception& ex)
        {
            VA_ENGINE_ERROR(
                "[MaterialSystem] Failed to parse material file '{}': {}", materialPath, ex.what());
        }

        // If we reach this point, something went wrong.
        return nullptr;
    }

    Resources::MaterialPtr MaterialSystem::CreateMaterial(const MaterialConfig& config)
    {
        // Create a new material based on the provided configuration
        Resources::IMaterial* material = nullptr;
        switch (Renderer::RenderCommand::GetApiType())
        {
            case Platform::RHI_API_TYPE::Vulkan:
            {
                material = Renderer::RenderCommand::GetRHIRef().CreateMaterial(
                    config.name, config.pipeline);
            }
            default:
                break;
        }
        if (!material)
        {
            VA_ENGINE_ERROR("[MaterialSystem] Failed to create material '{}'.", config.name);
            return nullptr;
        }

        // Set up the material with the config...
        // Diffuse color
        material->m_DiffuseColor = config.diffuseColor;

        // TODO Other colors

        // TODO We should have a more flexible way to setup textures.
        // Diffuse map
        if (!config.diffuseTexture.name.empty())
        {
            material->m_DiffuseTexture = g_TextureSystem->LoadTexture2D(
                config.diffuseTexture.name, config.diffuseTexture.use);
            if (material->m_DiffuseTexture == nullptr)
            {
                VA_ENGINE_WARN(
                    "[MaterialSystem] Failed to load diffuse texture '{}' for material '{}', using "
                    "default.",
                    config.diffuseTexture.name,
                    config.name);
                material->m_DiffuseTexture = g_TextureSystem->GetDefaultTexture();
            }
        }
        else
        {
            // No diffuse texture provided by the material.
            material->m_DiffuseTexture = nullptr;
        }

        // TODO Other maps

        // Initialize the material resources
        material->InitializeResources(Renderer::RenderCommand::GetRHIRef());

        auto materialPtr = Resources::MaterialPtr(material, MaterialDeleter{this});

        // Store the material in the cache
        m_MaterialCache[material->m_UUID] = materialPtr;

        return materialPtr;
    }

    void MaterialSystem::GenerateDefaultMaterials()
    {
        std::string name = "DefaultMaterial";

        Resources::IMaterial* material = nullptr;
        switch (Renderer::RenderCommand::GetApiType())
        {
            case Platform::RHI_API_TYPE::Vulkan:
            {
                material = Renderer::RenderCommand::GetRHIRef().CreateMaterial(
                    name, g_PipelineSystem->GetDefaultPipeline());
            }
            default:
                break;
        }
        if (!material)
        {
            VA_ENGINE_ERROR("[MaterialSystem] Failed to create material '{}'.", name);
            m_DefaultMaterial = nullptr;
            return;
        }

        // Set up the material with the config...
        // Diffuse color
        material->m_DiffuseColor = Math::Vec4::One();
        material->m_DiffuseTexture = g_TextureSystem->GetDefaultTexture();

        // Initialize the material resources
        material->InitializeResources(Renderer::RenderCommand::GetRHIRef());

        const auto materialPtr = Resources::MaterialPtr(material, MaterialDeleter{this});

        // Store the material in the cache
        m_MaterialCache[material->m_UUID] = materialPtr;

        m_DefaultMaterial = materialPtr;
    }

    uint32_t MaterialSystem::GetFreeMaterialHandle()
    {
        // First check if we have a free handle in queue.
        if (!m_FreeMaterialHandles.empty())
        {
            const auto handle = m_FreeMaterialHandles.front();
            m_FreeMaterialHandles.pop();
            return handle;
        }

        if (m_NextFreeMaterialHandle >= m_Materials.size())
        {
            m_Materials.resize(m_NextFreeMaterialHandle + 1);
        }
        return m_NextFreeMaterialHandle++;
    }

    void MaterialSystem::ReleaseMaterial(const Resources::IMaterial* material) {}

    void MaterialSystem::MaterialDeleter::operator()(const Resources::IMaterial* material) const
    {
        system->ReleaseMaterial(material);
        delete material;
    }

} // namespace VoidArchitect
