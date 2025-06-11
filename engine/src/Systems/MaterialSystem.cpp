#include "MaterialSystem.hpp"

#include "Core/Logger.hpp"
#include "Platform/RHI/IRenderingHardware.hpp"
#include "ResourceSystem.hpp"
#include "Resources/Loaders/MaterialLoader.hpp"
#include "Resources/Material.hpp"
#include "TextureSystem.hpp"
#include "Renderer/RenderSystem.hpp"
#include "Resources/Shader.hpp"

namespace VoidArchitect
{
    size_t MaterialTemplate::GetHash() const
    {
        size_t seed = 0;
        HashCombine(seed, name);
        HashCombine(seed, diffuseColor.X());
        HashCombine(seed, diffuseColor.Y());
        HashCombine(seed, diffuseColor.Z());
        HashCombine(seed, diffuseColor.W());
        HashCombine(seed, GetBindingsHash());
        HashCombine(seed, diffuseTexture.name);
        HashCombine(seed, specularTexture.name);
        HashCombine(seed, renderStateClass);
        return seed;
    }

    size_t MaterialTemplate::GetBindingsHash() const
    {
        size_t seed = 0;

        // Sort the bindings by their binding property
        auto bindings = resourceBindings;
        std::sort(bindings.begin(), bindings.end());
        for (auto& binding : bindings)
        {
            HashCombine(seed, binding.binding);
            HashCombine(seed, binding.type);
            HashCombine(seed, binding.stage);
        }

        return seed;
    }

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

    MaterialHandle MaterialSystem::GetHandleFor(const std::string& name)
    {
        // Check if the material is in the cache
        for (MaterialHandle handle = 0; handle < m_Materials.size(); ++handle)
        {
            if (const auto& node = m_Materials[handle]; node.config.name == name)
            {
                if (node.state == MaterialLoadingState::Loaded) return handle;

                // The material exist but is unloaded, we need to load it first.
                LoadMaterial(handle);
                return handle;
            }
        }

        // This is the first time the system is asked an handle for this material,
        // we have to load its config file from disk.
        const auto handle = LoadTemplate(name);
        LoadMaterial(handle);

        return handle;
    }

    Renderer::MaterialClass MaterialSystem::GetClass(const MaterialHandle handle) const
    {
        const auto& node = m_Materials[handle];
        if (node.config.renderStateClass.empty() || node.config.renderStateClass != "UI") return
            Renderer::MaterialClass::Standard;
        else return Renderer::MaterialClass::UI;
    }

    MaterialTemplate& MaterialSystem::GetTemplateFor(const MaterialHandle handle)
    {
        return m_Materials[handle].config;
    }

    Resources::IMaterial* MaterialSystem::GetPointerFor(const MaterialHandle handle) const
    {
        return m_Materials[handle].materialPtr;
    }

    void MaterialSystem::Bind(const MaterialHandle handle, const RenderStateHandle stateHandle)
    {
        // Is this handle valid?
        if (handle >= m_Materials.size())
        {
            VA_ENGINE_ERROR("[MaterialSystem] Invalid material handle.");
            return;
        }

        // Is the material loaded?
        if (m_Materials[handle].state != MaterialLoadingState::Loaded)
        {
            // This material is unloaded, load it now.
            LoadMaterial(handle);
        }

        // Bind the material
        Renderer::g_RenderSystem->GetRHI()->BindMaterial(handle, stateHandle);
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
        const auto materialData = g_ResourceSystem->LoadResource<
            Resources::Loaders::MaterialDataDefinition>(ResourceType::Material, name);
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

    void MaterialSystem::LoadMaterial(const MaterialHandle handle)
    {
        auto node = m_Materials[handle];
        if (node.state == MaterialLoadingState::Loaded) return;

        const auto material = CreateMaterial(node.config);
        if (!material)
        {
            VA_ENGINE_ERROR("[MaterialSystem] Failed to load material '{}'.", node.config.name);
        }

        VA_ENGINE_TRACE("[MaterialSystem] Loaded material '{}'.", node.config.name);
        m_Materials[handle].materialPtr = material;
        m_Materials[handle].state = MaterialLoadingState::Loaded;
    }

    Resources::IMaterial* MaterialSystem::CreateMaterial(const MaterialTemplate& matTemplate)
    {
        // Ask the RHI to create the required data on the GPU.
        const auto material = Renderer::g_RenderSystem->GetRHI()->CreateMaterial(
            matTemplate.name,
            matTemplate);
        if (!material)
        {
            VA_ENGINE_ERROR("[MaterialSystem] Failed to create material '{}'.", matTemplate.name);
            return nullptr;
        }

        // Set material properties from the template
        material->SetDiffuseColor(matTemplate.diffuseColor);

        // Load textures
        if (!matTemplate.diffuseTexture.name.empty())
        {
            if (const auto texture = g_TextureSystem->GetHandleFor(matTemplate.diffuseTexture.name))
            {
                material->SetTexture(matTemplate.diffuseTexture.use, texture);
            }
            else
            {
                VA_ENGINE_WARN(
                    "[MaterialSystem] Failed to load diffuse texture '{}' for material '{}', "
                    "using default.",
                    matTemplate.diffuseTexture.name,
                    matTemplate.name);
            }
        }

        if (!matTemplate.specularTexture.name.empty())
        {
            const auto texture = g_TextureSystem->GetHandleFor(matTemplate.specularTexture.name);
            if (texture)
            {
                material->SetTexture(matTemplate.specularTexture.use, texture);
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
            {Renderer::ResourceBindingType::ConstantBuffer, 0, Resources::ShaderStage::Pixel, {}},
            // MaterialUBO
            {Renderer::ResourceBindingType::Texture2D, 1, Resources::ShaderStage::Pixel, {}},
            // DiffuseMap
            {Renderer::ResourceBindingType::Texture2D, 2, Resources::ShaderStage::Pixel, {}},
            // SpecularMap
        };

        RegisterTemplate("DefaultMaterial", defaultTemplate);

        MaterialTemplate uiTemplate;
        uiTemplate.name = "DefaultUIMaterial";
        uiTemplate.diffuseColor = Math::Vec4::One();
        uiTemplate.renderStateClass = "UIState";

        uiTemplate.resourceBindings = {
            // MaterialUBO
            {Renderer::ResourceBindingType::ConstantBuffer, 0, Resources::ShaderStage::Pixel, {}},
            // DiffuseMap
            {Renderer::ResourceBindingType::Texture2D, 1, Resources::ShaderStage::Pixel, {}},
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
