//
// Created by Michael Desmedt on 01/06/2025.
//
#include "ResourceSystem.hpp"

#include <VoidArchitect/Engine/Common/Logger.hpp>

#include "Resources/Loaders/ImageLoader.hpp"
#include "Resources/Loaders/Loader.hpp"
#include "Resources/Loaders/MaterialLoader.hpp"
#include "Resources/Loaders/ShaderLoader.hpp"
#include "Resources/Loaders/VamLoader.hpp"

namespace VoidArchitect
{
    ResourceSystem::ResourceSystem()
    {
        // TODO : Make theses paths configurable
        const std::string BASE_ASSET_DIR = "../../../../../assets/";
        const std::string IMAGE_PATH = BASE_ASSET_DIR + "textures/";
        const std::string MATERIAL_PATH = BASE_ASSET_DIR + "materials/";
        const std::string SHADER_PATH = BASE_ASSET_DIR + "shaders/";
        const std::string MESH_PATH = BASE_ASSET_DIR + "meshes/";

        // Initialize default loaders
        RegisterLoader(ResourceType::Image, new Resources::Loaders::ImageLoader(IMAGE_PATH));
        RegisterLoader(
            ResourceType::Material,
            new Resources::Loaders::MaterialLoader(MATERIAL_PATH));
        RegisterLoader(ResourceType::Shader, new Resources::Loaders::ShaderLoader(SHADER_PATH));
        RegisterLoader(ResourceType::Mesh, new Resources::Loaders::VAMLoader(MESH_PATH));
    }

    void ResourceSystem::RegisterLoader(ResourceType type, Resources::Loaders::ILoader* loader)
    {
        // Check if a Systems already exists for this type
        // If so, warn the user and bail out
        // If not, register the Systems
        if (m_Loaders.contains(type))
        {
            VA_ENGINE_WARN(
                "[ResourceSystem] A Systems already exists for type: {}.",
                ResourceTypeToString(type));
            return;
        }

        m_Loaders[type] = std::unique_ptr<Resources::Loaders::ILoader>(loader);
        VA_ENGINE_DEBUG(
            "[ResourceSystem] Registered Systems for type: {}.",
            ResourceTypeToString(type));
    }

    void ResourceSystem::UnregisterLoader(const ResourceType type) { m_Loaders.erase(type); }

    std::string ResourceSystem::ResourceTypeToString(const ResourceType type)
    {
        switch (type)
        {
            case ResourceType::Image:
                return "Image";
            case ResourceType::Material:
                return "Material";
            case ResourceType::Shader:
                return "Shader";
            case ResourceType::Mesh:
                return "Mesh";
            default:
                break;
        }

        VA_ENGINE_WARN("[ResourceSystem] Unknown resource type.");
        return "Unknown";
    }

    template <typename T>
    std::shared_ptr<T> ResourceSystem::LoadResource(
        const ResourceType type,
        const std::string& path)
    {
        if (m_Loaders.contains(type))
        {
            return std::dynamic_pointer_cast<T>(m_Loaders[type]->Load(path));
        }

        VA_ENGINE_WARN(
            "[ResourceSystem] No Systems registered for type: {}.",
            ResourceTypeToString(type));
        return nullptr;
    }

    // === Templates ===
    // NOTE : These templates allow to load a specified resources type. If we define a new resource
    //          we must not forget to add a template specialization for it.
    //          This allow us to enforce the proper use of the LoadResource method. I.E. Not
    //          allowing to load a resource type that doesn't exist.
    template Resources::Loaders::ImageDataDefinitionPtr ResourceSystem::LoadResource<
        Resources::Loaders::ImageDataDefinition>(const ResourceType type, const std::string& name);

    template Resources::Loaders::MaterialDataDefinitionPtr ResourceSystem::LoadResource<
        Resources::Loaders::MaterialDataDefinition>(
        const ResourceType type,
        const std::string& name);

    template Resources::Loaders::ShaderDataDefinitionPtr ResourceSystem::LoadResource<
        Resources::Loaders::ShaderDataDefinition>(const ResourceType type, const std::string& name);

    template Resources::Loaders::MeshDataDefinitionPtr ResourceSystem::LoadResource<
        Resources::Loaders::MeshDataDefinition>(const ResourceType type, const std::string& name);
} // namespace VoidArchitect