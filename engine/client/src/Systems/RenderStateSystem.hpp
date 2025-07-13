//
// Created by Michael Desmedt on 27/05/2025.
//
#pragma once
#include "RenderPassSystem.hpp"

#include <VoidArchitect/Engine/RHI/Resources/RendererTypes.hpp>
#include <VoidArchitect/Engine/RHI/Resources/RenderPass.hpp>
#include <VoidArchitect/Engine/RHI/Resources/RenderState.hpp>
#include <VoidArchitect/Engine/RHI/Resources/Shader.hpp>

namespace VoidArchitect
{
    struct RenderStateCacheKey
    {
        Renderer::MaterialClass materialClass;
        Renderer::RenderPassType passType;
        Renderer::VertexFormat vertexFormat;
        Resources::RenderPassSignature passSignature;

        bool operator==(const RenderStateCacheKey& other) const;
        [[nodiscard]] size_t GetHash() const;
    };
} // namespace VoidArchitect

namespace std
{
    template <>
    struct hash<VoidArchitect::RenderStateCacheKey>
    {
        size_t operator()(const VoidArchitect::RenderStateCacheKey& key) const noexcept
        {
            return key.GetHash();
        }
    };
} // namespace std

namespace VoidArchitect
{
    class RenderStateSystem
    {
    public:
        RenderStateSystem();
        ~RenderStateSystem();

        void RegisterPermutation(const Resources::RenderStateConfig& config);
        Resources::RenderStateHandle GetHandleFor(
            const RenderStateCacheKey& key,
            Resources::RenderPassHandle passHandle);

        void Bind(Resources::RenderStateHandle handle);

        Resources::IRenderState* GetPointerFor(Resources::RenderStateHandle handle);
        const Resources::RenderStateConfig& GetConfigFor(Resources::RenderStateHandle handle);

    private:
        static Resources::IRenderState* CreateRenderState(
            Resources::RenderStateConfig& config,
            Resources::RenderPassHandle passHandle);

        void LoadDefaultRenderStates();

        Resources::RenderStateHandle GetFreeRenderStateHandle();

        enum class RenderStateLoadingState
        {
            Unloaded,
            Loading,
            Loaded
        };

        struct RenderStateData
        {
            RenderStateLoadingState state = RenderStateLoadingState::Unloaded;
            Resources::RenderStateConfig config;
            Resources::IRenderState* renderStatePtr = nullptr;
        };

        using ConfigLookupKey = std::tuple<
            Renderer::MaterialClass, Renderer::RenderPassType, Renderer::VertexFormat>;
        VAHashMap<ConfigLookupKey, Resources::RenderStateConfig> m_ConfigMap;

        VAArray<RenderStateData> m_RenderStates;
        VAHashMap<RenderStateCacheKey, Resources::RenderStateHandle> m_RenderStateCache;

        std::queue<Resources::RenderStateHandle> m_FreeRenderStateHandles;
        Resources::RenderStateHandle m_NextFreeRenderStateHandle = 0;
    };

    inline std::unique_ptr<RenderStateSystem> g_RenderStateSystem;
} // namespace VoidArchitect
