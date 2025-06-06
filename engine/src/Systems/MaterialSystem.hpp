#pragma once

#include "Resources/Material.hpp"
#include "Resources/RenderState.hpp"

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
        TextureConfig specularTexture;

        std::string renderStateTemplate = "Default";
        VAArray<Renderer::RenderPassType> compatiblePassTypes;

        VAArray<ResourceBindingType> requiredBinding;
        VAArray<AttributeType> requiredAttributes;
    };

    struct MaterialSignature
    {
        std::string templateName;
        Renderer::RenderPassType passType;
        UUID renderStateUUID;

        bool operator==(const MaterialSignature&) const;
        size_t GetHash() const;
    };

    struct MaterialCacheKey
    {
        std::string templateName;
        MaterialSignature signature;

        bool operator==(const MaterialCacheKey&) const;
        size_t GetHash() const;
    };
}

namespace std
{
    template <>
    struct hash<VoidArchitect::MaterialSignature>
    {
        size_t operator()(const VoidArchitect::MaterialSignature& signature) const noexcept
        {
            return signature.GetHash();
        }
    };

    template <>
    struct hash<VoidArchitect::MaterialCacheKey>
    {
        size_t operator()(const VoidArchitect::MaterialCacheKey& key) const noexcept
        {
            return key.GetHash();
        }
    };
}

namespace VoidArchitect
{
    class MaterialSystem
    {
    public:
        MaterialSystem();
        ~MaterialSystem();
        void LoadTemplate(const std::string& name);

        void RegisterMaterialTemplate(const std::string& name, const MaterialConfig& config);

        Resources::MaterialPtr CreateMaterial(
            const std::string& templateName,
            Renderer::RenderPassType passType,
            const Resources::RenderStatePtr& renderState);

        const MaterialConfig& GetMaterialTemplate(const std::string& name) const;
        Resources::MaterialPtr GetCachedMaterial(
            const std::string& name,
            Renderer::RenderPassType passType,
            UUID renderStateUUID) const;
        Resources::MaterialPtr GetDefaultMaterial() { return m_DefaultMaterial; }

    private:
        void GenerateDefaultMaterials();
        uint32_t GetFreeMaterialHandle();
        void ReleaseMaterial(const Resources::IMaterial* material);

        bool ValidateCompatibility(
            const MaterialConfig& config,
            Renderer::RenderPassType passType,
            const Resources::RenderStatePtr& renderState);

        MaterialSignature CreateSignature(
            const std::string& templateName,
            Renderer::RenderPassType passType,
            const Resources::RenderStatePtr& renderState);

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

        VAArray<MaterialData> m_Materials;

        VAHashMap<std::string, MaterialConfig> m_MaterialTemplates;
        VAHashMap<MaterialCacheKey, std::weak_ptr<Resources::IMaterial>> m_MaterialCache;

        Resources::MaterialPtr m_DefaultMaterial;
    };

    inline std::unique_ptr<MaterialSystem> g_MaterialSystem;
} // namespace VoidArchitect
