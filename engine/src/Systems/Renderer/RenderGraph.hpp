//
// Created by Michael Desmedt on 02/06/2025.
//
#pragma once
#include <memory>

#include "Core/Math/Mat4.hpp"
#include "Core/Math/Vec4.hpp"
#include "Resources/Pipeline.hpp"
#include "Resources/RenderPass.hpp"

namespace VoidArchitect
{
    namespace Platform
    {
        class IRenderingHardware;
    }

    namespace Resources
    {
        class Texture2D;
        using Texture2DPtr = std::shared_ptr<Texture2D>;
    }

    namespace Renderer
    {
        struct FrameData
        {
            float deltaTime;
            Math::Mat4 View;
            Math::Mat4 Projection;
        };

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

        const char* RenderPassTypeToString(RenderPassType type);

        struct RenderPassConfig
        {
            std::string Name;
            RenderPassType Type = RenderPassType::Unknown;

            std::vector<std::string> CompatiblePipelines;

            struct AttachmentConfig
            {
                std::string Name;

                TextureFormat Format;
                LoadOp LoadOp = LoadOp::Clear;
                StoreOp StoreOp = StoreOp::Store;

                // Clear values (used if LoadOp is Clear)
                Math::Vec4 ClearColor = Math::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
                float ClearDepth = 1.0f;
                uint32_t ClearStencil = 0;
            };

            std::vector<AttachmentConfig> Attachments;
        };

        struct RenderTargetConfig
        {
            std::string Name;

            uint32_t Width;
            uint32_t Height;

            TextureFormat Format;
            bool IsMain = false;

            // If provided, use these instead of creating new ones
            std::vector<Resources::Texture2DPtr> Attachments;
        };

        class RenderGraph
        {
        public:
            explicit RenderGraph(Platform::IRenderingHardware& rhi);
            ~RenderGraph();

            // Graph construction
            Resources::RenderPassPtr AddRenderPass(const RenderPassConfig& config);
            Resources::RenderTargetPtr AddRenderTarget(const RenderTargetConfig& config);
            void AddDependency(Resources::RenderPassPtr from, Resources::RenderPassPtr to);

            // Connect passes to targets
            void ConnectPassToTarget(
                const Resources::RenderPassPtr& pass,
                const Resources::RenderTargetPtr& target);

            // Graph lifecycle
            bool Validate();
            bool Compile();
            void Execute(const FrameData& frameData);

            // Compilation process
            bool CompileRenderPasses();
            bool CompilePipelines();

            // Resize handling
            void OnResize(uint32_t width, uint32_t height);

            // Convenience methods for common setups
            void SetupForwardRenderer(uint32_t width, uint32_t height);

            //Debug/introspection
            const std::string& GetRenderPassName(Resources::RenderPassPtr pass) const;
            const std::string& GetRenderTargetName(Resources::RenderTargetPtr target) const;

        private:
            struct RenderPassNode
            {
                RenderPassConfig Config;
                Resources::RenderPassPtr RenderPass;
                std::vector<UUID> DependenciesUUIDs;
                std::vector<UUID> OutputsUUIDs;
            };

            struct RenderTargetNode
            {
                RenderTargetConfig Config;
                Resources::RenderTargetPtr RenderTarget;
            };

            RenderPassNode* FindRenderPassNode(const UUID& passUUID);
            RenderPassNode* FindRenderPassNode(Resources::RenderPassPtr pass);
            RenderTargetNode* FindRenderTargetNode(const UUID& targetUUID);
            RenderTargetNode* FindRenderTargetNode(Resources::RenderTargetPtr target);

            void ReleaseRenderPass(const Resources::IRenderPass* pass);
            void ReleaseRenderTarget(const Resources::IRenderTarget* target);

            // Internal methods
            bool ValidateNoCycles();
            bool ValidateReferences();
            bool ValidatePassPipelineCompatibility();

            std::vector<Resources::RenderPassPtr> GetExecutionOrder();
            void CreateRHIResources();

            // Pass execution
            void RenderPassContent(
                Resources::RenderPassPtr pass,
                Resources::RenderTargetPtr target,
                const FrameData& frameData);

            void RenderForwardPass(
                const RenderPassConfig& passConfig,
                const Resources::PipelinePtr& pipeline,
                const FrameData&
                frameData);
            void RenderShadowPass(
                const RenderPassConfig& passConfig,
                const Resources::PipelinePtr& pipeline,
                const FrameData&
                frameData);
            void RenderDepthPrepassPass(
                const RenderPassConfig& passConfig,
                const Resources::PipelinePtr& pipeline,
                const FrameData& frameData);
            void RenderPostProcessPass(
                const RenderPassConfig& passConfig,
                const Resources::PipelinePtr& pipeline,
                const FrameData& frameData);
            void RenderUIPass(
                const RenderPassConfig& passConfig,
                const Resources::PipelinePtr& pipeline,
                const FrameData&
                frameData);

            Platform::IRenderingHardware& m_RHI;

            // Graph data
            std::unordered_map<UUID, RenderPassNode> m_RenderPassesNodes;
            std::unordered_map<UUID, RenderTargetNode> m_RenderTargetsNodes;

            std::vector<Resources::RenderPassPtr> m_ExecutionOrder;

            // State
            bool m_IsCompiled = false;
            bool m_IsDestroying = false;
            uint32_t m_CurrentWidth = 0;
            uint32_t m_CurrentHeight = 0;
        };

        inline std::unique_ptr<RenderGraph> g_RenderGraph;
    }
}
