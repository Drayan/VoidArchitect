//
// Created by Michael Desmedt on 04/06/2025.
//
#pragma once

#include "Core/Uuid.hpp"
#include "Core/Math/Vec4.hpp"
#include "Resources/RenderPass.hpp"

namespace VoidArchitect
{
    namespace Renderer
    {
        enum class LoadOp
        {
            Load,
            Clear,
            DontCare
        };

        enum class StoreOp
        {
            Store,
            DontCare
        };

        // TODO : Add support for more formats
        enum class TextureFormat
        {
            RGBA8_UNORM,
            BGRA8_UNORM,
            RGBA8_SRGB,
            BGRA8_SRGB,

            D32_SFLOAT,
            D24_UNORM_S8_UINT,

            SWAPCHAIN_FORMAT,
            SWAPCHAIN_DEPTH
        };

        enum class RenderPassType
        {
            ForwardOpaque,
            ForwardTransparent,
            Shadow,
            DepthPrepass,
            PostProcess,
            UI,
            Unknown
        };
    } // namespace Renderer

    struct RenderPassConfig
    {
        std::string Name;
        Renderer::RenderPassType Type = Renderer::RenderPassType::Unknown;

        std::vector<std::string> CompatibleStates;

        struct AttachmentConfig
        {
            std::string Name;

            Renderer::TextureFormat Format;
            Renderer::LoadOp LoadOp = Renderer::LoadOp::Clear;
            Renderer::StoreOp StoreOp = Renderer::StoreOp::Store;

            // Clear values (used if LoadOp is Clear)
            Math::Vec4 ClearColor = Math::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
            float ClearDepth = 1.0f;
            uint32_t ClearStencil = 0;
        };

        std::vector<AttachmentConfig> Attachments;
    };

    struct RenderPassSignature
    {
        std::vector<Renderer::TextureFormat> AttachmentFormats;

        bool operator==(const RenderPassSignature& rhs) const;
        size_t GetHash() const;
    };

    struct RenderPassCacheKey
    {
        UUID templateUUID;
        RenderPassSignature Signature;

        bool operator==(const RenderPassCacheKey& rhs) const;
        size_t GetHash() const;
    };
}

namespace std
{
    template <>
    struct hash<VoidArchitect::RenderPassSignature>
    {
        size_t operator()(const VoidArchitect::RenderPassSignature& signature) const noexcept
        {
            return signature.GetHash();
        }
    };

    template <>
    struct hash<VoidArchitect::RenderPassCacheKey>
    {
        size_t operator()(const VoidArchitect::RenderPassCacheKey& key) const noexcept
        {
            return key.GetHash();
        }
    };
}

namespace VoidArchitect
{
    class RenderPassSystem
    {
    public:
        RenderPassSystem();
        ~RenderPassSystem() = default;

        // Template management
        UUID RegisterRenderPassTemplate(const std::string& name, const RenderPassConfig& config);
        bool HasRenderPassTemplate(const UUID& uuid) const;
        bool HasRenderPassTemplate(const std::string& name) const;
        const RenderPassConfig& GetRenderPassTemplate(const UUID& uuid) const;
        UUID GetRenderPassTemplateUUID(const std::string& name) const;

        // Creation with caching
        Resources::RenderPassPtr CreateRenderPass(
            const UUID& templateUUID);

        Resources::RenderPassPtr GetCachedRenderPass(
            const UUID& templateUUID,
            const RenderPassSignature& signature);

        RenderPassSignature CreateSignature(const RenderPassConfig& config);

        void ClearCache();

    private:
        void GenerateDefaultRenderPasses();

        struct RenderPassTemplate
        {
            UUID UUID;
            std::string Name;
            RenderPassConfig Config;
        };

        // Templates storage
        std::unordered_map<UUID, RenderPassTemplate> m_RenderPassTemplates;
        std::unordered_map<std::string, UUID> m_TemplatesNameToUUIDMap;

        // Cache based on signature
        std::unordered_map<RenderPassCacheKey, Resources::RenderPassPtr> m_RenderPassCache;
    };

    inline std::unique_ptr<RenderPassSystem> g_RenderPassSystem;
} // namespace VoidArchitect

