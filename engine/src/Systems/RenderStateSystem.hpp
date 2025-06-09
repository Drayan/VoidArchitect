//
// Created by Michael Desmedt on 27/05/2025.
//
#pragma once
#include "RenderPassSystem.hpp"
#include "Renderer/RendererTypes.hpp"
#include "Resources/RenderPass.hpp"
#include "Resources/RenderState.hpp"
#include "Resources/Shader.hpp"

namespace VoidArchitect
{
    struct RenderStateConfig
    {
        std::string name;

        std::string renderStateClass;
        Renderer::RenderPassType passType;
        Renderer::VertexFormat vertexFormat = Renderer::VertexFormat::Position;

        VAArray<Resources::ShaderPtr> shaders;
        VAArray<Renderer::VertexAttribute> vertexAttributes;
        // TODO InputLayout; -> Which data bindings are used?
        Renderer::RenderStateInputLayout inputLayout;
        // TODO RenderState -> Allow configuration options like culling, depth testing, etc.
    };

    struct RenderStateCacheKey
    {
        std::string renderStateClass;
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
    using RenderStateHandle = uint32_t;
    static constexpr RenderStateHandle InvalidRenderStateHandle = std::numeric_limits<
        uint32_t>::max();

    class RenderStateSystem
    {
    public:
        RenderStateSystem();
        ~RenderStateSystem() = default;

        void RegisterPermutation(const RenderStateConfig& config);
        RenderStateHandle GetHandleFor(
            const RenderStateCacheKey& key,
            RenderPassHandle passHandle);

        void Bind(RenderStateHandle handle);

    private:
        static Resources::IRenderState* CreateRenderState(
            RenderStateConfig& config,
            RenderPassHandle passHandle);

        void LoadDefaultRenderStates();

        RenderStateHandle GetFreeRenderStateHandle();

        enum class RenderStateLoadingState
        {
            Unloaded,
            Loading,
            Loaded
        };

        struct RenderStateData
        {
            RenderStateLoadingState state = RenderStateLoadingState::Unloaded;
            Resources::IRenderState* renderStatePtr = nullptr;
        };

        using ConfigLookupKey = std::tuple<
            std::string, Renderer::RenderPassType, Renderer::VertexFormat>;
        VAHashMap<ConfigLookupKey, RenderStateConfig> m_ConfigMap;

        VAArray<RenderStateData> m_RenderStates;
        VAHashMap<RenderStateCacheKey, RenderStateHandle> m_RenderStateCache;

        std::queue<RenderStateHandle> m_FreeRenderStateHandles;
        RenderStateHandle m_NextFreeRenderStateHandle = 0;
    };

    inline std::unique_ptr<RenderStateSystem> g_RenderStateSystem;
} // namespace VoidArchitect
