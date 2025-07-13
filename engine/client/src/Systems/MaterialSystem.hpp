#pragma once

#include "RenderStateSystem.hpp"

#include <VoidArchitect/Engine/RHI/Resources/Material.hpp>
#include <VoidArchitect/Engine/RHI/Resources/RenderState.hpp>

namespace VoidArchitect
{
    class MaterialSystem
    {
    public:
        MaterialSystem();
        ~MaterialSystem();

        MaterialHandle GetHandleFor(const std::string& name);
        MaterialHandle GetHandleForDefaultMaterial() { return GetHandleFor("DefaultMaterial"); }
        Renderer::MaterialClass GetClass(MaterialHandle handle) const;
        MaterialHandle RegisterTemplate(const std::string& name, const MaterialTemplate& config);

        MaterialTemplate& GetTemplateFor(MaterialHandle handle);
        Resources::IMaterial* GetPointerFor(MaterialHandle handle);;

        // Interaction with Material
        void Bind(MaterialHandle handle, Resources::RenderStateHandle stateHandle);

    private:
        MaterialHandle LoadTemplate(const std::string& name);

        void LoadMaterial(MaterialHandle handle);
        //void UnloadMaterial(MaterialHandle handle);

        void LoadDefaultMaterials();

        MaterialHandle GetFreeMaterialHandle();

        static Resources::IMaterial* CreateMaterial(const MaterialTemplate& matTemplate);

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
