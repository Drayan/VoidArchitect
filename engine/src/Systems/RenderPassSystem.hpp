//
// Created by Michael Desmedt on 04/06/2025.
//
#pragma once

#include "Core/Utils.hpp"
#include "Renderer/RendererTypes.hpp"
#include "Resources/RenderPass.hpp"

namespace VoidArchitect
{
    namespace Renderer
    {
        enum class PassPosition;
    }

    struct RenderPassCacheKey
    {
        Renderer::RenderPassConfig config;
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
    struct hash<VoidArchitect::Renderer::RenderPassConfig::AttachmentConfig>
    {
        size_t operator()(
            const VoidArchitect::Renderer::RenderPassConfig::AttachmentConfig& config) const
            noexcept
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
    struct hash<VoidArchitect::Renderer::RenderPassConfig>
    {
        size_t operator()(const VoidArchitect::Renderer::RenderPassConfig& config) const noexcept
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
        size_t operator()(const VoidArchitect::RenderPassCacheKey& key) const noexcept
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
            const Renderer::RenderPassConfig& config,
            Renderer::PassPosition position);

        void ReleasePass(RenderPassHandle handle);

        [[nodiscard]] Resources::IRenderPass* GetPointerFor(uint32_t handle) const;

    private:
        static Resources::IRenderPass* CreateRenderPass(
            const Renderer::RenderPassConfig& config,
            Renderer::PassPosition passPosition);
        RenderPassHandle GetFreeHandle();

        VAArray<std::unique_ptr<Resources::IRenderPass>> m_RenderPasses;
        VAHashMap<RenderPassCacheKey, RenderPassHandle> m_RenderPassCache;

        std::queue<RenderPassHandle> m_FreeRenderPassHandles;
        RenderPassHandle m_NextFreeRenderPassHandle = 0;
    };

    inline std::unique_ptr<RenderPassSystem> g_RenderPassSystem;
} // namespace VoidArchitect


