#include "MaterialSystem.hpp"

#include "Core/Logger.hpp"
#include "Platform/RHI/IRenderingHardware.hpp"
#include "RenderStateSystem.hpp"
#include "Renderer/RenderCommand.hpp"
#include "ResourceSystem.hpp"
#include "Resources/Loaders/MaterialLoader.hpp"
#include "Resources/Material.hpp"
#include "TextureSystem.hpp"
#include "Renderer/RenderGraph.hpp"

namespace VoidArchitect
{
    MaterialSystem::MaterialSystem()
    {
        GenerateDefaultMaterials();
    }

    MaterialSystem::~MaterialSystem()
    {
    }

    void MaterialSystem::LoadTemplate(const std::string& name)
    {
        if (m_MaterialTemplates.contains(name))
        {
            VA_ENGINE_WARN("[MaterialSystem] Material template '{}' already exists.", name);
            return;
        }

        const auto materialData =
            g_ResourceSystem->LoadResource<Resources::Loaders::MaterialDataDefinition>(
                ResourceType::Material,
                name);
        if (!materialData)
        {
            VA_ENGINE_ERROR("[MaterialSystem] Failed to load material template '{}'.", name);
        }

        RegisterMaterialTemplate(name, materialData->GetConfig());
    }

    void MaterialSystem::RegisterMaterialTemplate(
        const std::string& name,
        const MaterialConfig& config)
    {
        if (m_MaterialTemplates.contains(name))
        {
            VA_ENGINE_WARN(
                "[MaterialSystem] Material template '{}' already exists. Overwriting.",
                name);
        }

        m_MaterialTemplates[name] = config;
        VA_ENGINE_TRACE("[MaterialSystem] Registered material template '{}'.", name);
    }

    Resources::MaterialPtr MaterialSystem::CreateMaterial(
        const std::string& templateName,
        Renderer::RenderPassType passType,
        const Resources::RenderStatePtr& renderState)
    {
        // Find the template
        const auto templateIt = m_MaterialTemplates.find(templateName);
        if (templateIt == m_MaterialTemplates.end())
        {
            VA_ENGINE_ERROR(
                "[MaterialSystem] Failed to create material, template '{}' not found.",
                templateName);
            return nullptr;
        }

        const auto& materialTemplate = templateIt->second;

        // Validate compatibility
        if (!ValidateCompatibility(materialTemplate, passType, renderState))
        {
            VA_ENGINE_ERROR(
                "[MaterialSystem] Failed to create material, template '{}' is not compatible "
                "with render state '{}'.",
                templateName,
                renderState->GetName());
            return nullptr;
        }

        // Create cache key and check cache
        const auto signature = CreateSignature(templateName, passType, renderState);
        const MaterialCacheKey cacheKey{templateName, signature};

        if (const auto cacheIt = m_MaterialCache.find(cacheKey); cacheIt != m_MaterialCache.end())
        {
            if (auto cached = cacheIt->second.lock())
            {
                VA_ENGINE_TRACE(
                    "[MaterialSystem] Reusing cached material '{}'.",
                    cached->m_Name);
                return cached;
            }

            // Remove expired weak pointers from the cache
            m_MaterialCache.erase(cacheKey);
        }

        // Create new material instance
        const auto material = Renderer::RenderCommand::GetRHIRef().CreateMaterial(
            materialTemplate.name);
        if (!material)
        {
            VA_ENGINE_ERROR(
                "[MaterialSystem] Failed to create material '{}'.",
                materialTemplate.name);
            return nullptr;
        }

        // Set material properties from template
        material->SetDiffuseColor(materialTemplate.diffuseColor);

        // Load textures
        if (!materialTemplate.diffuseTexture.name.empty())
        {
            const auto texture = g_TextureSystem->LoadTexture2D(
                materialTemplate.diffuseTexture.name,
                Resources::TextureUse::Diffuse);
            if (texture)
            {
                material->SetTexture(0, texture);
            }
            else
            {
                VA_ENGINE_WARN(
                    "[MaterialSystem] Failed to load diffuse texture '{}' for material '{}', "
                    "using default.",
                    materialTemplate.diffuseTexture.name,
                    materialTemplate.name
                );
            }
        }

        if (!materialTemplate.specularTexture.name.empty())
        {
            const auto texture = g_TextureSystem->LoadTexture2D(
                materialTemplate.specularTexture.name,
                Resources::TextureUse::Specular);
            if (texture)
            {
                material->SetTexture(1, texture);
            }
            else
            {
                VA_ENGINE_WARN(
                    "[MaterialSystem] Failed to load specular texture '{}' for material '{}', "
                    "using default.",
                    materialTemplate.specularTexture.name,
                    materialTemplate.name);
            }
        }

        // TODO: Load other textures

        // Initialize resources with RenderState
        material->InitializeResources(Renderer::RenderCommand::GetRHIRef(), renderState);

        // Cache the material
        auto materialPtr = Resources::MaterialPtr(material);
        m_MaterialCache[cacheKey] = materialPtr;

        VA_ENGINE_TRACE(
            "[MaterialSystem] Created material '{}' for pass type '{}'.",
            material->m_Name,
            Renderer::RenderPassTypeToString(passType));
        return materialPtr;
    }

    Resources::MaterialPtr MaterialSystem::GetCachedMaterial(
        const std::string& name,
        const Renderer::RenderPassType passType,
        const UUID renderStateUUID) const
    {
        const auto signature = MaterialSignature(name, passType, renderStateUUID);
        const MaterialCacheKey cacheKey{name, signature};
        if (const auto cacheIt = m_MaterialCache.find(cacheKey); cacheIt != m_MaterialCache.end())
        {
            if (auto cached = cacheIt->second.lock())
            {
                return cached;
            }
        }

        return nullptr;
    }

    // Resources::MaterialPtr MaterialSystem::LoadMaterial(const std::string& name)
    // {
    //     // Check if the material is already loaded in the cache
    //     for (auto& [uuid, material] : m_MaterialCache)
    //     {
    //         const auto& mat = material.lock();
    //         if (mat && mat->m_Name == name)
    //         {
    //             // If the material is found in the cache, return it
    //             return mat;
    //         }
    //
    //         if (mat == nullptr)
    //         {
    //             // Remove expired weak pointers from the cache
    //             m_MaterialCache.erase(uuid);
    //         }
    //     }
    //
    //     const auto materialData =
    //         g_ResourceSystem->LoadResource<Resources::Loaders::MaterialDataDefinition>(
    //             ResourceType::Material,
    //             name);
    //     // If we reach this point, something went wrong.
    //     return CreateMaterial(materialData->GetConfig());
    // }

    bool MaterialSystem::ValidateCompatibility(
        const MaterialConfig& config,
        Renderer::RenderPassType passType,
        const Resources::RenderStatePtr& renderState)
    {
        //TODO: Implement validation logic
        //  Check if RenderState InputLayout contains required bindings
        //  Check if RenderState InputLayout contains required attributes
        const auto& compatiblePassTypes = config.compatiblePassTypes;
        bool passTypeSupported = std::ranges::find(compatiblePassTypes, passType) !=
            compatiblePassTypes.end();

        if (!passTypeSupported)
        {
            VA_ENGINE_WARN(
                "[MaterialSystem] Material template '{}' does not support pass type '{}'.",
                config.name,
                Renderer::RenderPassTypeToString(passType));
        }

        return true;
    }

    MaterialSignature MaterialSystem::CreateSignature(
        const std::string& templateName,
        Renderer::RenderPassType passType,
        const Resources::RenderStatePtr& renderState)
    {
        return MaterialSignature(templateName, passType, renderState->GetUUID());
    }

    void MaterialSystem::GenerateDefaultMaterials()
    {
        MaterialConfig defaultTemplate;
        defaultTemplate.name = "DefaultMaterial";
        defaultTemplate.diffuseColor = Math::Vec4::One();
        defaultTemplate.renderStateTemplate = "Default";
        defaultTemplate.compatiblePassTypes = {Renderer::RenderPassType::ForwardOpaque};

        RegisterMaterialTemplate("Default", defaultTemplate);

        MaterialConfig uiTemplate;
        uiTemplate.name = "DefaultUIMaterial";
        uiTemplate.diffuseColor = Math::Vec4::One();
        uiTemplate.renderStateTemplate = "UI";
        uiTemplate.compatiblePassTypes = {Renderer::RenderPassType::UI};

        RegisterMaterialTemplate("DefaultUIMaterial", uiTemplate);

        VA_ENGINE_INFO("[MaterialSystem] Generated default materials.");
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

    bool MaterialSignature::operator==(const MaterialSignature& other) const
    {
        return templateName == other.templateName && renderStateUUID == other.renderStateUUID &&
            passType == other.passType;
    }

    size_t MaterialSignature::GetHash() const
    {
        return std::hash<std::string>{}(templateName) ^ std::hash<UUID>{}(renderStateUUID) ^
            std::hash<Renderer::RenderPassType>{}(passType);
    }

    bool MaterialCacheKey::operator==(const MaterialCacheKey& other) const
    {
        return templateName == other.templateName && signature == other.signature;
    }

    size_t MaterialCacheKey::GetHash() const
    {
        return std::hash<std::string>{}(templateName) ^ signature.GetHash();
    }
} // namespace VoidArchitect
