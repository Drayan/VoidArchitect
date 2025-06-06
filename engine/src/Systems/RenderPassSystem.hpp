//
// Created by Michael Desmedt on 04/06/2025.
//
#pragma once

#include "Core/Uuid.hpp"
#include "Core/Math/Vec4.hpp"
#include "Renderer/PassRenderers.hpp"

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

        enum class PassPosition
        {
            First, // UNDEFINED -> COLOR_ATTACHMENT
            Middle, // COLOR_ATTACHMENT -> COLOR_ATTACHMENT
            Last, // COLOR_ATTACHMENT -> PRESENT
            Standalone // UNDEFINED -> PRESENT
        };
    } // namespace Renderer

    struct RenderPassConfig
    {
        std::string Name;
        Renderer::RenderPassType Type = Renderer::RenderPassType::Unknown;

        VAArray<std::string> CompatibleStates;
        Renderer::PassPosition Position = Renderer::PassPosition::Standalone;

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

        VAArray<AttachmentConfig> Attachments;
    };

    struct RenderPassSignature
    {
        VAArray<Renderer::TextureFormat> AttachmentFormats;

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

        // Pass Renderer Management
        void RegisterPassRenderer(Renderer::PassRendererPtr& renderer);
        Renderer::IPassRenderer* GetPassRenderer(Renderer::RenderPassType type) const;

        const VAArray<std::string>& GetAvailableRenderers() const
        {
            return m_AvailableRenderersNames;
        }

        // Creation with caching
        Resources::RenderPassPtr CreateRenderPass(
            const UUID& templateUUID,
            Renderer::PassPosition passPosition);

        Resources::RenderPassPtr GetCachedRenderPass(
            const UUID& templateUUID,
            const RenderPassSignature& signature);

        RenderPassSignature CreateSignature(const RenderPassConfig& config);

        void ClearCache();

    private:
        void GenerateDefaultRenderPasses();
        void RegisterDefaultRenderers();

        struct RenderPassTemplate
        {
            UUID UUID;
            std::string Name;
            RenderPassConfig Config;
        };

        // Templates storage
        VAHashMap<UUID, RenderPassTemplate> m_RenderPassTemplates;
        VAHashMap<std::string, UUID> m_TemplatesNameToUUIDMap;

        // Cache based on signature
        VAHashMap<RenderPassCacheKey, Resources::RenderPassPtr> m_RenderPassCache;

        // Pass renderers
        VAHashMap<Renderer::RenderPassType, Renderer::PassRendererPtr> m_PassRenderers;
        VAArray<std::string> m_AvailableRenderersNames;
    };

    inline std::unique_ptr<RenderPassSystem> g_RenderPassSystem;
} // namespace VoidArchitect

