#include "MaterialSystem.hpp"

#include "Core/Logger.hpp"
#include "PipelineSystem.hpp"
#include "Platform/RHI/IRenderingHardware.hpp"
#include "Renderer/RenderCommand.hpp"
#include "Resources/Material.hpp"
#include "TextureSystem.hpp"
#include "ResourceSystem.hpp"
#include "Resources/Loaders/MaterialLoader.hpp"

namespace VoidArchitect
{
    MaterialSystem::MaterialSystem()
    {
        m_Materials.reserve(
            1024); // Reserve space for 1024 materials to avoid frequent reallocations
        GenerateDefaultMaterials();
    }

    MaterialSystem::~MaterialSystem()
    {
    }

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

        const auto materialData = g_ResourceSystem->LoadResource<
            Resources::Loaders::MaterialDataDefinition>(ResourceType::Material, name);
        // If we reach this point, something went wrong.
        return CreateMaterial(materialData->GetConfig());
    }

    Resources::MaterialPtr MaterialSystem::CreateMaterial(const MaterialConfig& config)
    {
        // Create a new material based on the provided configuration
        Resources::IMaterial* material = nullptr;
        switch (Renderer::RenderCommand::GetApiType())
        {
            case Platform::RHI_API_TYPE::Vulkan:
            {
                material = Renderer::RenderCommand::GetRHIRef().CreateMaterial(config.name);
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
                config.diffuseTexture.name,
                config.diffuseTexture.use);
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
                material = Renderer::RenderCommand::GetRHIRef().CreateMaterial(name);
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

    void MaterialSystem::ReleaseMaterial(const Resources::IMaterial* material)
    {
    }

    void MaterialSystem::MaterialDeleter::operator()(const Resources::IMaterial* material) const
    {
        system->ReleaseMaterial(material);
        delete material;
    }
} // namespace VoidArchitect
