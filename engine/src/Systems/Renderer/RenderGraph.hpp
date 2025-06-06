//
// Created by Michael Desmedt on 02/06/2025.
//
#pragma once
#include "Core/Math/Mat4.hpp"
#include "Resources/Material.hpp"
#include "Resources/RenderPass.hpp"
#include "Resources/RenderState.hpp"
#include "Systems/RenderPassSystem.hpp"

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
    } // namespace Resources

    namespace Renderer
    {
        enum class TextureFormat;
        enum class RenderPassType;

        struct FrameData
        {
            float deltaTime;
            Math::Mat4 View;
            Math::Mat4 Projection;
        };

        const char* RenderPassTypeToString(RenderPassType type);

        struct RenderTargetConfig
        {
            std::string Name;

            uint32_t Width;
            uint32_t Height;

            TextureFormat Format;
            bool IsMain = false;

            // If provided, use these instead of creating new ones
            VAArray<Resources::Texture2DPtr> Attachments;
        };

        class RenderGraph
        {
        public:
            explicit RenderGraph(Platform::IRenderingHardware& rhi);
            ~RenderGraph();

            // Graph construction
            UUID AddRenderPass(const UUID& templateUUID, const std::string& instanceName = "");
            UUID AddRenderTarget(const RenderTargetConfig& config);
            void AddDependency(const UUID& fromUUID, const UUID& toUUID);

            // Connect passes to targets
            void ConnectPassToTarget(const UUID& passUUID, const UUID& targetUUID);

            // Graph lifecycle
            bool Validate();
            bool Compile();
            void Execute(const FrameData& frameData);

            // Resources requests
            void RegisterPassMapping(
                RenderPassType passType,
                const std::string& renderPassName,
                const RenderStateSignature& signature);
            void RequestMaterialForPassType(
                const std::string& identifier,
                const std::string& templateName,
                const std::string& renderStateTemplate,
                RenderPassType passType);

            // Compiled resources accessors
            Resources::RenderPassPtr GetRenderPass(const UUID& passUUID) const;
            Resources::RenderTargetPtr GetRenderTarget(const UUID& targetUUID) const;
            Resources::MaterialPtr GetMaterial(const std::string& identifier) const;

            bool IsCompiled() const { return m_IsCompiled; }

            // Resize handling
            void OnResize(uint32_t width, uint32_t height);

            // Convenience methods for common setups
            void SetupForwardRenderer(uint32_t width, uint32_t height);

        private:
            struct RenderPassNode
            {
                UUID instanceUUID;
                UUID templateUUID;
                std::string instanceName;
                Resources::RenderPassPtr RenderPass;
                VAArray<UUID> DependenciesUUIDs;
                VAArray<UUID> OutputsUUIDs;

                PassPosition ComputedPosition;

                std::string RequiredStateTemplateName;
                Resources::RenderStatePtr AssignedState;
            };

            struct RenderTargetNode
            {
                UUID instanceUUID;
                RenderTargetConfig Config;
                Resources::RenderTargetPtr RenderTarget;
            };

            struct PassMapping
            {
                RenderPassType passType;
                std::string renderPassName;
                RenderStateSignature signature;
            };

            struct MaterialRequest
            {
                std::string templateName;
                std::string renderStateTemplate;
                std::string identifier;
                RenderPassType passType;
            };

            RenderPassNode* FindRenderPassNode(const UUID& instanceUUID);
            RenderTargetNode* FindRenderTargetNode(const UUID& instanceUUID);
            RenderPassNode* FindRenderPassNode(Resources::RenderPassPtr& pass);
            RenderTargetNode* FindRenderTargetNode(Resources::RenderTargetPtr& target);
            const RenderPassNode* FindRenderPassNode(const UUID& instanceUUID) const;
            const RenderTargetNode* FindRenderTargetNode(const UUID& instanceUUID) const;
            const RenderPassNode* FindRenderPassNode(const Resources::RenderPassPtr& pass) const;
            const RenderTargetNode* FindRenderTargetNode(
                const Resources::RenderTargetPtr& target) const;

            void ReleaseRenderPass(const Resources::IRenderPass* pass);
            void ReleaseRenderTarget(const Resources::IRenderTarget* target);

            // Graph validation
            bool ValidateNoCycles();
            bool ValidateConnections();
            bool ValidatePassRenderStateCompatibility();

            // Graph compilation
            bool CompileRenderPasses();
            bool CompileRenderTargets();
            bool CompileRenderStates();
            bool CompileMaterials();
            void AssignRequiredStates();
            void OptimizeExecutionOrder();
            float CalculateStateSwitchCost() const;

            VAArray<UUID> ComputeExecutionOrder();

            // Pass execution
            void RenderPassContent(
                const Resources::RenderPassPtr& pass,
                const Resources::RenderTargetPtr& target,
                const FrameData& frameData);

            void BindRenderStateIfNeeded(const Resources::RenderStatePtr& newState);
            void LogOptimizationMetrics() const;

            // Graph data
            VAHashMap<UUID, RenderPassNode> m_RenderPassesNodes;
            VAHashMap<UUID, RenderTargetNode> m_RenderTargetsNodes;

            VAArray<UUID> m_ExecutionOrder;

            // Material resources
            VAArray<PassMapping> m_PassMappings;
            VAArray<MaterialRequest> m_MaterialRequests;
            VAHashMap<std::string, Resources::MaterialPtr> m_CompiledMaterials;

            // State
            Platform::IRenderingHardware& m_RHI;
            Resources::RenderStatePtr m_LastBoundState;
            bool m_IsCompiled = false;
            bool m_IsDestroying = false;
            uint32_t m_CurrentWidth = 0;
            uint32_t m_CurrentHeight = 0;
            uint32_t m_StateChangeCount = 0;
        };

        inline std::unique_ptr<RenderGraph> g_RenderGraph;
    } // namespace Renderer
} // namespace VoidArchitect
