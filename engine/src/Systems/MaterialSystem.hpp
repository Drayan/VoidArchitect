#pragma once

#include "RenderStateSystem.hpp"
#include "Resources/Material.hpp"
#include "Resources/RenderState.hpp"

namespace VoidArchitect
{
    struct MaterialTemplate
    {
        std::string name;
        std::string renderStateClass;

        Math::Vec4 diffuseColor = Math::Vec4::One();

        VAArray<Renderer::ResourceBinding> resourceBindings;

        struct TextureConfig
        {
            std::string name;
            Resources::TextureUse use;
        };

        TextureConfig diffuseTexture;
        TextureConfig specularTexture;

        [[nodiscard]] size_t GetHash() const;
        size_t GetBindingsHash() const;
    };

    class MaterialSystem
    {
    public:
        MaterialSystem();
        ~MaterialSystem();

        MaterialHandle GetHandleFor(const std::string& name);
        MaterialHandle GetHandleForDefaultMaterial() { return 0; }
        Renderer::MaterialClass GetClass(MaterialHandle handle) const;

        MaterialTemplate& GetTemplateFor(MaterialHandle handle);
        Resources::IMaterial* GetPointerFor(MaterialHandle handle) const;;

        // Interaction with Material
        void Bind(MaterialHandle handle, RenderStateHandle stateHandle);

    private:
        MaterialHandle LoadTemplate(const std::string& name);
        MaterialHandle RegisterTemplate(const std::string& name, const MaterialTemplate& config);

        void LoadMaterial(MaterialHandle handle);
        //void UnloadMaterial(MaterialHandle handle);

        void LoadDefaultMaterials();

        MaterialHandle GetFreeMaterialHandle();

        static Resources::IMaterial* CreateMaterial(const MaterialTemplate& matTemplate);

        void ReleaseMaterial(const Resources::IMaterial* material);

        enum class MaterialLoadingState
        {
            Unloaded,
            Loading,
            Loaded
        };

        struct MaterialData
        {
            UUID uuid = InvalidUUID;
            MaterialTemplate config;
            MaterialLoadingState state = MaterialLoadingState::Unloaded;

            Resources::IMaterial* materialPtr = nullptr;
        };

        std::queue<MaterialHandle> m_FreeMaterialHandles;
        MaterialHandle m_NextFreeMaterialHandle = 0;

        VAArray<MaterialData> m_Materials;
    };

    inline std::unique_ptr<MaterialSystem> g_MaterialSystem;
} // namespace VoidArchitect
