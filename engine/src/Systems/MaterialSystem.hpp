#pragma once

#include "Resources/Material.hpp"
#include "Resources/Pipeline.hpp"

namespace VoidArchitect
{
    struct MaterialConfig
    {
        std::string name;

        Math::Vec4 diffuseColor = Math::Vec4::One();

        struct TextureConfig
        {
            std::string name;
            Resources::TextureUse use;
        };

        TextureConfig diffuseTexture;

        Resources::PipelinePtr pipeline;
    };

    class MaterialSystem
    {
    public:
        MaterialSystem();
        ~MaterialSystem();

        Resources::MaterialPtr LoadMaterial(const std::string& name);
        Resources::MaterialPtr CreateMaterial(const MaterialConfig& config);

        Resources::MaterialPtr GetDefaultMaterial() { return m_DefaultMaterial; }

    private:
        void GenerateDefaultMaterials();
        uint32_t GetFreeMaterialHandle();
        void ReleaseMaterial(const Resources::IMaterial* material);

        struct MaterialDeleter
        {
            MaterialSystem* system;
            void operator()(const Resources::IMaterial* material) const;
        };

        struct MaterialData
        {
            UUID uuid = InvalidUUID;
        };

        std::queue<uint32_t> m_FreeMaterialHandles;
        uint32_t m_NextFreeMaterialHandle = 0;

        std::vector<MaterialData> m_Materials;
        std::unordered_map<UUID, std::weak_ptr<Resources::IMaterial>> m_MaterialCache;

        Resources::MaterialPtr m_DefaultMaterial;
    };

    inline std::unique_ptr<MaterialSystem> g_MaterialSystem;
} // namespace VoidArchitect
