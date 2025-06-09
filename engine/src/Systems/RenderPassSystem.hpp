//
// Created by Michael Desmedt on 04/06/2025.
//
#pragma once

#include "Core/Utils.hpp"
#include "Core/Math/Vec4.hpp"
#include "Renderer/RendererTypes.hpp"
#include "Resources/RenderPass.hpp"

namespace VoidArchitect
{
    namespace Renderer
    {
        enum class PassPosition;
    }

    struct RenderPassConfig
    {
        std::string name;
        Renderer::RenderPassType type = Renderer::RenderPassType::Unknown;

        struct AttachmentConfig
        {
            std::string name;

            Renderer::TextureFormat format;
            Renderer::LoadOp loadOp = Renderer::LoadOp::Clear;
            Renderer::StoreOp storeOp = Renderer::StoreOp::Store;

            // Clear values (used if LoadOp is Clear)
            Math::Vec4 clearColor = Math::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
            float clearDepth = 1.0f;
            uint32_t clearStencil = 0;

            bool operator==(const AttachmentConfig&) const;
        };

        VAArray<AttachmentConfig> attachments;

        bool operator==(const RenderPassConfig&) const;
    };

    struct RenderPassCacheKey
    {
        RenderPassConfig config;
        Renderer::PassPosition position;

        bool operator==(const RenderPassCacheKey&) const;
    };

    using RenderPassHandle = uint32_t;
    static constexpr RenderPassHandle InvalidRenderPassHandle = std::numeric_limits<
        uint32_t>::max();
}

namespace std
{
    template <>
    struct hash<VoidArchitect::RenderPassConfig::AttachmentConfig>
    {
        size_t operator()(
            const VoidArchitect::RenderPassConfig::AttachmentConfig& config) const noexcept
        {
            size_t seed = 0;
            VoidArchitect::HashCombine(seed, config.name);
            VoidArchitect::HashCombine(seed, static_cast<int>(config.format));
            VoidArchitect::HashCombine(seed, static_cast<int>(config.loadOp));
            VoidArchitect::HashCombine(seed, static_cast<int>(config.storeOp));

            return seed;
        }
    };

    template <>
    struct hash<VoidArchitect::RenderPassConfig>
    {
        size_t operator()(
            const VoidArchitect::RenderPassConfig& config) const noexcept
        {
            size_t seed = 0;
            VoidArchitect::HashCombine(seed, config.name);
            VoidArchitect::HashCombine(seed, static_cast<int>(config.type));
            for (const auto& attachment : config.attachments)
            {
                VoidArchitect::HashCombine(seed, attachment);
            }

            return seed;
        }
    };

    template <>
    struct hash<VoidArchitect::RenderPassCacheKey>
    {
        size_t operator()(
            const VoidArchitect::RenderPassCacheKey& key) const noexcept
        {
            size_t seed = 0;
            VoidArchitect::HashCombine(seed, key.config);
            VoidArchitect::HashCombine(seed, static_cast<int>(key.position));

            return seed;
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

        RenderPassHandle GetHandleFor(
            const RenderPassConfig& config,
            Renderer::PassPosition position);

        void ReleasePass(RenderPassHandle handle);

        Resources::IRenderPass* GetPointerFor(uint32_t handle) const;

    private:
        static Resources::IRenderPass* CreateRenderPass(
            const RenderPassConfig& config,
            Renderer::PassPosition passPosition);
        RenderPassHandle GetFreeHandle();

        VAArray<std::unique_ptr<Resources::IRenderPass>> m_RenderPasses;
        VAHashMap<RenderPassCacheKey, RenderPassHandle> m_RenderPassCache;

        std::queue<RenderPassHandle> m_FreeRenderPassHandles;
        RenderPassHandle m_NextFreeRenderPassHandle = 0;
    };

    inline std::unique_ptr<RenderPassSystem> g_RenderPassSystem;
} // namespace VoidArchitect


