#include "MaterialSystem.hpp"

#include "Core/Logger.hpp"
#include "Platform/RHI/IRenderingHardware.hpp"
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
        m_Materials.reserve(256); // Reserve some space for the material nodes
        LoadDefaultMaterials();
    }

    MaterialSystem::~MaterialSystem()
    {
        for (MaterialHandle handle = 0; handle < m_Materials.size(); ++handle)
        {
            if (m_Materials[handle].state == MaterialLoadingState::Loaded)
            {
                delete m_Materials[handle].materialPtr;
            }
        }
        m_Materials.clear();
    }

    MaterialHandle MaterialSystem::LoadTemplate(const std::string& name)
    {
        // Check if the material is already registered in the system, if so, return its handle.
        for (MaterialHandle handle = 0; handle < m_Materials.size(); ++handle)
        {
            if (const auto& node = m_Materials[handle]; node.config.name == name)
            {
                VA_ENGINE_WARN("[MaterialSystem] Material template '{}' already exists.", name);
                return handle;
            }
        }

        // Load the MaterialTemplate from the disk
        const auto materialData =
            g_ResourceSystem->LoadResource<Resources::Loaders::MaterialDataDefinition>(
                ResourceType::Material,
                name);
        if (!materialData)
        {
            VA_ENGINE_ERROR("[MaterialSystem] Failed to load material template '{}'.", name);
            return InvalidMaterialHandle;
        }

        return RegisterTemplate(name, materialData->GetConfig());
    }

    MaterialHandle MaterialSystem::RegisterTemplate(
        const std::string& name,
        const MaterialTemplate& config)
    {
        // Check if the material is already registered in the system, if so, return its handle.
        for (MaterialHandle handle = 0; handle < m_Materials.size(); ++handle)
        {
            if (const auto& node = m_Materials[handle]; node.config.name == name)
            {
                VA_ENGINE_WARN("[MaterialSystem] Material template '{}' already exists.", name);
                return handle;
            }
        }

        const MaterialData node{InvalidUUID, config, MaterialLoadingState::Unloaded, nullptr};
        const MaterialHandle handle = GetFreeMaterialHandle();
        m_Materials[handle] = node;

        VA_ENGINE_TRACE("[MaterialSystem] Registered material template '{}'.", name);
        return handle;
    }

    void MaterialSystem::Get(MaterialHandle handle)
    {
        // Check if the material is 1. In the system and 2. is already loaded
        if (handle >= m_Materials.size())
        {
            VA_ENGINE_ERROR("[MaterialSystem] Invalid material handle '{}'.", handle);
        }

        if (m_Materials[handle].state == MaterialLoadingState::Loaded)
            return;

        // NOTE: This is where we could launch an AsyncJob to load the material in the background
        //  But for now we will do it synchronously.
        const auto& node = m_Materials[handle];
        const auto material = CreateMaterial(node.config);
        if (!material)
        {
            VA_ENGINE_ERROR(
                "[MaterialSystem] Failed to load material '{}'.",
                node.config.name);
        }

        VA_ENGINE_TRACE("[MaterialSystem] Loaded material '{}'.", node.config.name);
        m_Materials[handle].materialPtr = material;
        m_Materials[handle].state = MaterialLoadingState::Loaded;
    }

    Resources::IMaterial* MaterialSystem::CreateMaterial(
        const MaterialTemplate& matTemplate)
    {
        // Ask the RHI to create the required data on the GPU.
        const auto material = Renderer::RenderCommand::GetRHIRef().CreateMaterial(
            matTemplate.name);
        if (!material)
        {
            VA_ENGINE_ERROR(
                "[MaterialSystem] Failed to create material '{}'.",
                matTemplate.name);
            return nullptr;
        }

        // Set material properties from the template
        material->SetDiffuseColor(matTemplate.diffuseColor);

        // Load textures
        if (!matTemplate.diffuseTexture.name.empty())
        {
            const auto texture = g_TextureSystem->LoadTexture2D(
                matTemplate.diffuseTexture.name,
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
                    matTemplate.diffuseTexture.name,
                    matTemplate.name
                );
            }
        }

        if (!matTemplate.specularTexture.name.empty())
        {
            const auto texture = g_TextureSystem->LoadTexture2D(
                matTemplate.specularTexture.name,
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
                    matTemplate.specularTexture.name,
                    matTemplate.name);
            }
        }

        // TODO: Load other textures

        // Initialize resources with RenderState
        material->InitializeResources(
            Renderer::RenderCommand::GetRHIRef(),
            matTemplate.resourceBindings);

        return material;
    }

    void MaterialSystem::LoadDefaultMaterials()
    {
        MaterialTemplate defaultTemplate;
        defaultTemplate.name = "DefaultMaterial";
        defaultTemplate.diffuseColor = Math::Vec4::One();
        defaultTemplate.renderStateClass = "DefaultState";

        // Define the DefaultMaterial bindings
        defaultTemplate.resourceBindings = {
            {ResourceBindingType::ConstantBuffer, 0, Resources::ShaderStage::Pixel, {}},
            // MaterialUBO
            {ResourceBindingType::Texture2D, 1, Resources::ShaderStage::Pixel, {}},
            // DiffuseMap
            {ResourceBindingType::Texture2D, 2, Resources::ShaderStage::Pixel, {}},
            // SpecularMap
        };

        RegisterTemplate("DefaultMaterial", defaultTemplate);

        MaterialTemplate uiTemplate;
        uiTemplate.name = "DefaultUIMaterial";
        uiTemplate.diffuseColor = Math::Vec4::One();
        uiTemplate.renderStateClass = "UIState";

        uiTemplate.resourceBindings = {
            // MaterialUBO
            {ResourceBindingType::ConstantBuffer, 0, Resources::ShaderStage::Pixel, {}},
            // DiffuseMap
            {ResourceBindingType::Texture2D, 1, Resources::ShaderStage::Pixel, {}},
        };

        RegisterTemplate("DefaultUIMaterial", uiTemplate);
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
} // namespace VoidArchitect
