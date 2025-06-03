//
// Created by Michael Desmedt on 02/06/2025.
//
#include "RenderGraph.hpp"

#include <ranges>

#include "RenderCommand.hpp"
#include "Core/Logger.hpp"
#include "Platform/RHI/IRenderingHardware.hpp"
#include "Resources/Material.hpp"
#include "Resources/RenderTarget.hpp"
#include "Systems/MaterialSystem.hpp"
#include "Systems/PipelineSystem.hpp"

namespace VoidArchitect::Renderer
{
    const char* RenderPassTypeToString(const RenderPassType type)
    {
        switch (type)
        {
            case RenderPassType::Unknown:
                return "Unknown";
            case RenderPassType::ForwardOpaque:
                return "ForwardOpaque";
            case RenderPassType::ForwardTransparent:
                return "ForwardTransparent";
            case RenderPassType::DepthPrepass:
                return "DepthPrepass";
            case RenderPassType::Shadow:
                return "Shadow";
            case RenderPassType::PostProcess:
                return "PostProcess";
            case RenderPassType::UI:
                return "UI";
            default:
                return "Invalid";
        }
    }

    RenderGraph::RenderGraph(Platform::IRenderingHardware& rhi)
        : m_RHI(rhi)
    {
        m_RenderPassesNodes.reserve(16);
        m_RenderTargetsNodes.reserve(8);
    }

    RenderGraph::~RenderGraph()
    {
        VA_ENGINE_TRACE("[RenderGraph] Destroying RenderGraph...");

        m_IsDestroying = true;

        m_ExecutionOrder.clear();
        m_RenderTargetsNodes.clear();
        m_RenderPassesNodes.clear();

        VA_ENGINE_TRACE("[RenderGraph] RenderGraph destroyed.");
    }

    Resources::RenderPassPtr RenderGraph::AddRenderPass(const RenderPassConfig& config)
    {
        // Create the render pass
        Resources::IRenderPass* rawPass = m_RHI.CreateRenderPass(config);
        if (!rawPass)
        {
            VA_ENGINE_ERROR("[RenderGraph] Failed to create RenderPass '{}'.", config.Name);
            return nullptr;
        }

        // Wrap in smart pointer with custom deleter
        const auto renderPass = Resources::RenderPassPtr(
            rawPass,
            [this](Resources::IRenderPass* rawPass)
            {
                this->ReleaseRenderPass(rawPass);
                delete rawPass;
            });

        // Create and store node
        RenderPassNode node;
        node.Config = config;
        node.RenderPass = renderPass;

        const auto passUUID = rawPass->GetUUID();
        m_RenderPassesNodes[passUUID] = std::move(node);

        m_IsCompiled = false;

        VA_ENGINE_TRACE("[RenderGraph] RenderPass '{}' added.", config.Name);
        return renderPass;
    }

    Resources::RenderTargetPtr RenderGraph::AddRenderTarget(const RenderTargetConfig& config)
    {
        // Create the render target
        Resources::IRenderTarget* rawTarget = m_RHI.CreateRenderTarget(config);
        if (!rawTarget)
        {
            VA_ENGINE_ERROR("[RenderGraph] Failed to create RenderTarget '{}'.", config.Name);
            return nullptr;
        }

        // Wrap in a smart pointer with custom deleter
        const auto renderTarget = Resources::RenderTargetPtr(
            rawTarget,
            [this](Resources::IRenderTarget* rawTarget)
            {
                this->ReleaseRenderTarget(rawTarget);
                delete rawTarget;
            });

        // Create and store node
        RenderTargetNode node;
        node.Config = config;
        node.RenderTarget = renderTarget;

        auto targetUUID = rawTarget->GetUUID();
        m_RenderTargetsNodes[targetUUID] = std::move(node);

        m_IsCompiled = false;

        VA_ENGINE_TRACE("[RenderGraph] RenderTarget '{}' added.", config.Name);
        return renderTarget;
    }

    void RenderGraph::AddDependency(Resources::RenderPassPtr from, Resources::RenderPassPtr to)
    {
        if (!from || !to)
        {
            VA_ENGINE_ERROR("[RenderGraph] Cannot add dependency between invalid RenderPass.");
            return;
        }

        const auto fromUUID = from->GetUUID();
        const auto toUUID = to->GetUUID();

        // Find the 'from' node
        auto* fromNode = FindRenderPassNode(fromUUID);
        if (!fromNode)
        {
            VA_ENGINE_ERROR(
                "[RenderGraph] Cannot find RenderPass node for '{}' for dependency source.",
                from->GetName());
            return;
        }

        // Check if dependency already exists
        auto& dependencies = fromNode->DependenciesUUIDs;
        if (std::ranges::find(dependencies, toUUID) != dependencies.end())
        {
            VA_ENGINE_WARN(
                "[RenderGraph] Dependency already exists between RenderPass '{}' -> '{}'.",
                from->GetName(),
                to->GetName());
            return;
        }

        dependencies.push_back(toUUID);

        VA_ENGINE_TRACE(
            "[RenderGraph] Dependency added between RenderPass '{}' -> '{}'.",
            from->GetName(),
            to->GetName());
    }

    void RenderGraph::ConnectPassToTarget(
        const Resources::RenderPassPtr& pass,
        const Resources::RenderTargetPtr& target)
    {
        if (!pass || !target)
        {
            VA_ENGINE_ERROR(
                "[RenderGraph] Cannot connect RenderPass to RenderTarget, at least one of them is invalid.");
            return;
        }

        const auto passUUID = pass->GetUUID();
        const auto targetUUID = target->GetUUID();

        auto* passNode = FindRenderPassNode(passUUID);
        if (!passNode)
        {
            VA_ENGINE_ERROR(
                "[RenderGraph] Cannot find RenderPass node for '{}' for target connection.",
                pass->GetName());
            return;
        }

        // Validate compatibility
        if (!pass->IsCompatibleWith(target))
        {
            VA_ENGINE_ERROR(
                "[RenderGraph] RenderPass '{}' is not compatible with RenderTarget '{}'.",
                pass->GetName(),
                target->GetName());
            return;
        }

        // Check if target already connected
        auto& outputs = passNode->OutputsUUIDs;
        if (std::ranges::find(outputs, targetUUID) != outputs.end())
        {
            VA_ENGINE_WARN(
                "[RenderGraph] RenderPass '{}' already connected to RenderTarget '{}'.",
                pass->GetName(),
                target->GetName());
            return;
        }

        outputs.push_back(targetUUID);

        m_IsCompiled = false;

        VA_ENGINE_TRACE(
            "[RenderGraph] RenderPass '{}' connected to RenderTarget '{}'.",
            pass->GetName(),
            target->GetName());
    }

    bool RenderGraph::Validate()
    {
        if (m_RenderPassesNodes.empty())
        {
            VA_ENGINE_ERROR("[RenderGraph] No RenderPass added to the graph.");
            return false;
        }

        if (m_RenderTargetsNodes.empty())
        {
            VA_ENGINE_ERROR("[RenderGraph] No RenderTarget added to the graph.");
            return false;
        }

        // Validate that all passes have outputs
        for (const auto& passNode : m_RenderPassesNodes | std::views::values)
        {
            if (passNode.OutputsUUIDs.empty())
            {
                VA_ENGINE_ERROR(
                    "[RenderGraph] RenderPass '{}' has no output.",
                    passNode.Config.Name);
                return false;
            }
        }

        if (!ValidatePassPipelineCompatibility())
        {
            VA_ENGINE_ERROR("[RenderGraph] Cannot find a compatible pipeline for passes.");
            return false;
        }

        // TODO: Add more validation as needed
        //   - ValidateNoCycles()
        //   - ValidateReferences()
        //   - Validate attachement compatibility

        VA_ENGINE_TRACE("[RenderGraph] Graph validated.");
        return true;
    }

    bool RenderGraph::Compile()
    {
        VA_ENGINE_INFO("[RenderGraph] Compiling graph...");

        // Validate the graph
        if (!Validate())
        {
            VA_ENGINE_ERROR("[RenderGraph] Graph validation failed, cannot compile.");
            return false;
        }

        // Compute execution order
        m_ExecutionOrder = GetExecutionOrder();

        if (m_ExecutionOrder.empty())
        {
            VA_ENGINE_ERROR("[RenderGraph] Failed to compute execution order.");
            return false;
        }

        if (!CompileRenderPasses())
        {
            VA_ENGINE_ERROR("[RenderGraph] Failed to compile RenderPasses.");
            return false;
        }

        if (!CompilePipelines())
        {
            VA_ENGINE_ERROR("[RenderGraph] Failed to compile Pipelines.");
            return false;
        }

        // Mark as compiled
        m_IsCompiled = true;

        VA_ENGINE_INFO("[RenderGraph] Graph compiled successfully. Execution order:");
        for (size_t i = 0; i < m_ExecutionOrder.size(); ++i)
        {
            if (const auto* passNode = FindRenderPassNode(m_ExecutionOrder[i]->GetUUID()))
            {
                VA_ENGINE_INFO(
                    "[RenderGraph]   {}: '{}' (Type: {})",
                    i,
                    m_ExecutionOrder[i]->GetName(),
                    RenderPassTypeToString(passNode->Config.Type));
            }
            else
            {
                VA_ENGINE_INFO("[RenderGraph]   {}: '{}'", i, m_ExecutionOrder[i]->GetName());
            }
        }

        return true;
    }

    void RenderGraph::Execute(const FrameData& frameData)
    {
        if (!m_IsCompiled)
        {
            VA_ENGINE_ERROR("[RenderGraph] Graph is not compiled, cannot execute.");
            return;
        }

        // VA_ENGINE_TRACE(
        //     "[RenderGraph] Executing render graph with {} passes.",
        //     m_ExecutionOrder.size());

        // Execute passes in order (using cached execution order for performance)
        for (auto& pass : m_ExecutionOrder)
        {
            auto* passNode = FindRenderPassNode(pass->GetUUID());
            if (!passNode)
            {
                VA_ENGINE_ERROR(
                    "[RenderGraph] Failed to find RenderPass node for pass '{}'.",
                    pass->GetName());
                continue;
            }

            if (passNode->OutputsUUIDs.empty())
            {
                VA_ENGINE_ERROR(
                    "[RenderGraph] RenderPass '{}' has no output.",
                    pass->GetName());
                continue;
            }

            // For now, assume single output per pass
            auto& targetUUID = passNode->OutputsUUIDs[0];
            auto* targetNode = FindRenderTargetNode(targetUUID);
            if (!targetNode)
            {
                VA_ENGINE_ERROR(
                    "[RenderGraph] Failed to find RenderTarget node for pass '{}'.",
                    pass->GetName());
                continue;
            }

            auto& renderTarget = targetNode->RenderTarget;

            // VA_ENGINE_TRACE(
            //     "[RenderGraph] Executing pass '{}' -> target '{}'.",
            //     pass->GetName(),
            //     renderTarget->GetName());

            pass->Begin(m_RHI, renderTarget);
            RenderPassContent(pass, renderTarget, frameData);
            pass->End(m_RHI);
        }

        // VA_ENGINE_TRACE("[RenderGraph] Render graph executed successfully.");
    }

    bool RenderGraph::CompileRenderPasses()
    {
        VA_ENGINE_DEBUG("[RenderGraph] Compiling RenderPasses...");

        // This step makes the real RHI RenderPass objects.
        for (const auto& passNode : m_RenderPassesNodes | std::views::values)
        {
            if (!passNode.RenderPass)
            {
                VA_ENGINE_ERROR(
                    "[RenderGraph] RenderPass '{}' is null during compilation.",
                    passNode.Config.Name);
                return false;
            }

            VA_ENGINE_TRACE("[RenderGraph] RenderPass '{}' compiled.", passNode.Config.Name);
        }

        VA_ENGINE_DEBUG("[RenderGraph] RenderPasses compiled successfully.");
        return true;
    }

    bool RenderGraph::CompilePipelines()
    {
        VA_ENGINE_DEBUG("[RenderGraph] Compiling Pipelines...");

        for (const auto& passNode : m_RenderPassesNodes | std::views::values)
        {
            const auto& passConfig = passNode.Config;
            const auto& renderPass = passNode.RenderPass;

            // For each compatible pipeline with this pass
            for (const auto& pipelineName : passConfig.CompatiblePipelines)
            {
                // Check that the template exists
                if (!g_PipelineSystem->HasPipelineTemplate(pipelineName))
                {
                    VA_ENGINE_ERROR(
                        "[RenderGraph] Pipeline template '{}' not found for pass '{}'.",
                        pipelineName,
                        passConfig.Name);
                    return false;
                }

                // Create the pipeline for this pass
                const auto pipeline = g_PipelineSystem->CreatePipelineForPass(
                    pipelineName,
                    passConfig,
                    renderPass);
                if (!pipeline)
                {
                    VA_ENGINE_ERROR(
                        "[RenderGraph] Failed to create pipeline for pass '{}'.",
                        passConfig.Name);
                    return false;
                }

                VA_ENGINE_TRACE(
                    "[RenderGraph] Pipeline '{}' compiled for pass '{}'.",
                    pipelineName,
                    passConfig.Name);
            }
        }

        VA_ENGINE_DEBUG("[RenderGraph] Pipelines compiled successfully.");
        return true;
    }

    void RenderGraph::OnResize(uint32_t width, uint32_t height)
    {
        if (width == m_CurrentWidth && height == m_CurrentHeight)
            return;

        VA_ENGINE_DEBUG(
            "[RenderGraph] Resize from {}x{} to {}x{}.",
            m_CurrentWidth,
            m_CurrentHeight,
            width,
            height);

        m_CurrentWidth = width;
        m_CurrentHeight = height;

        m_RHI.Resize(width, height);

        // Resize all main render targets
        for (auto& node : m_RenderTargetsNodes | std::views::values)
        {
            if (node.RenderTarget->IsMainTarget())
            {
                node.RenderTarget->Resize(width, height);

                // Update config for consistency
                node.Config.Width = width;
                node.Config.Height = height;

                VA_ENGINE_TRACE(
                    "[RenderGraph] Resized main RenderTarget '{}'.",
                    node.Config.Name);
            }
        }

        m_IsCompiled = false;
    }

    void RenderGraph::SetupForwardRenderer(uint32_t width, uint32_t height)
    {
        VA_ENGINE_INFO("[RenderGraph] Setting up Forward Renderer ({}x{}).", width, height);

        // Store current dimensions
        m_CurrentWidth = width;
        m_CurrentHeight = height;

        // === 1. Create Main Render Target (swapchain) ===
        RenderTargetConfig mainTargetConfig;
        mainTargetConfig.Name = "MainTarget";
        mainTargetConfig.Width = width;
        mainTargetConfig.Height = height;
        mainTargetConfig.Format = TextureFormat::SWAPCHAIN_FORMAT;
        mainTargetConfig.IsMain = true;

        auto mainTarget = AddRenderTarget(mainTargetConfig);
        if (!mainTarget)
        {
            VA_ENGINE_CRITICAL("[RenderGraph] Failed to create Main RenderTarget.");
            return;
        }

        // === 2. Create Forward Render Pass ===
        RenderPassConfig forwardPassConfig;
        forwardPassConfig.Name = "ForwardPass";
        forwardPassConfig.Type = RenderPassType::ForwardOpaque;
        forwardPassConfig.CompatiblePipelines = {"Default"};

        // Color attachment (swapchain)
        RenderPassConfig::AttachmentConfig colorAttachment;
        colorAttachment.Name = "color";
        colorAttachment.Format = TextureFormat::SWAPCHAIN_FORMAT;
        colorAttachment.LoadOp = LoadOp::Clear;
        colorAttachment.StoreOp = StoreOp::Store;
        colorAttachment.ClearColor = Math::Vec4(0.2f, 0.2f, 0.2f, 1.0f);

        // Depth attachment
        RenderPassConfig::AttachmentConfig depthAttachment;
        depthAttachment.Name = "depth";
        depthAttachment.Format = TextureFormat::SWAPCHAIN_DEPTH;
        depthAttachment.LoadOp = LoadOp::Clear;
        depthAttachment.StoreOp = StoreOp::DontCare;
        depthAttachment.ClearDepth = 1.0f;
        depthAttachment.ClearStencil = 0;

        forwardPassConfig.Attachments = {colorAttachment, depthAttachment};

        auto forwardPass = AddRenderPass(forwardPassConfig);
        if (!forwardPass)
        {
            VA_ENGINE_CRITICAL("[RenderGraph] Failed to create Forward RenderPass.");
            return;
        }

        // === 3. Connect Pass to Target ===
        ConnectPassToTarget(forwardPass, mainTarget);

        VA_ENGINE_INFO("[RenderGraph] Forward Renderer setup complete.");
        VA_ENGINE_INFO(
            "[RenderGraph]   - Main Target: '{}' ({}x{})",
            mainTarget->GetName(),
            width,
            height);
        VA_ENGINE_INFO(
            "[RenderGraph]   - Forward Pass: '{}' (Type: {}) with {} attachments and {} compatible pipelines.",
            forwardPass->GetName(),
            RenderPassTypeToString(forwardPassConfig.Type),
            forwardPassConfig.Attachments.size(),
            forwardPassConfig.CompatiblePipelines.size());
    }

    RenderGraph::RenderPassNode* RenderGraph::FindRenderPassNode(const UUID& passUUID)
    {
        const auto it = m_RenderPassesNodes.find(passUUID);
        return (it != m_RenderPassesNodes.end()) ? &it->second : nullptr;
    }

    RenderGraph::RenderPassNode* RenderGraph::FindRenderPassNode(Resources::RenderPassPtr pass)
    {
        return pass ? FindRenderPassNode(pass->GetUUID()) : nullptr;
    }

    RenderGraph::RenderTargetNode* RenderGraph::FindRenderTargetNode(const UUID& targetUUID)
    {
        auto it = m_RenderTargetsNodes.find(targetUUID);
        return (it != m_RenderTargetsNodes.end()) ? &it->second : nullptr;
    }

    RenderGraph::RenderTargetNode* RenderGraph::FindRenderTargetNode(
        Resources::RenderTargetPtr target)
    {
        return target ? FindRenderTargetNode(target->GetUUID()) : nullptr;
    }

    const std::string& RenderGraph::GetRenderPassName(Resources::RenderPassPtr pass) const
    {
        if (!pass)
        {
            static const std::string nullName = "NullPass";
            return nullName;
        }

        auto it = m_RenderPassesNodes.find(pass->GetUUID());
        if (it != m_RenderPassesNodes.end())
        {
            return it->second.Config.Name;
        }

        static const std::string unknown = "Unknown";
        return unknown;
    }

    const std::string& RenderGraph::GetRenderTargetName(Resources::RenderTargetPtr target) const
    {
        if (!target)
        {
            static const std::string nullName = "NullTarget";
            return nullName;
        }

        auto it = m_RenderTargetsNodes.find(target->GetUUID());
        if (it != m_RenderTargetsNodes.end())
        {
            return it->second.Config.Name;
        }

        static const std::string unknown = "Unknown";
        return unknown;
    }

    void RenderGraph::ReleaseRenderPass(const Resources::IRenderPass* pass)
    {
        if (!pass || m_IsDestroying) return;

        auto passUUID = pass->GetUUID();

        m_RenderPassesNodes.erase(passUUID);

        // Cleanup dependencies pointing to this pass
        for (auto& node : m_RenderPassesNodes | std::views::values)
        {
            auto& dependencies = node.DependenciesUUIDs;
            std::erase(dependencies, passUUID);
        }

        // Clear compilation state
        m_IsCompiled = false;
    }

    void RenderGraph::ReleaseRenderTarget(const Resources::IRenderTarget* target)
    {
        if (!target || m_IsDestroying) return;

        auto targetUUID = target->GetUUID();

        if (auto it = m_RenderTargetsNodes.find(targetUUID); it != m_RenderTargetsNodes.end())
        {
            it->second.RenderTarget = nullptr;
            m_RenderTargetsNodes.erase(it);
        }

        // Remove from all pass outputs
        for (auto& node : m_RenderPassesNodes | std::views::values)
        {
            auto& outputs = node.OutputsUUIDs;
            std::erase(outputs, targetUUID);
        }

        m_IsCompiled = false;
    }

    bool RenderGraph::ValidatePassPipelineCompatibility()
    {
        for (const auto& [uuid, passNode] : m_RenderPassesNodes)
        {
            const auto& passConfig = passNode.Config;

            if (passConfig.Type == RenderPassType::Unknown)
            {
                VA_ENGINE_WARN(
                    "[RenderGraph] RenderPass '{}' has unknown type.",
                    passConfig.Name);
                return false;
            }

            // Check that all pipelines exist and are compatible
            for (const auto& pipelineName : passConfig.CompatiblePipelines)
            {
                // For now, just log declared compatibility
                // Later, we will check with the PipelineSystem
                VA_ENGINE_DEBUG(
                    "[RenderGraph]   - Pipeline '{}' is compatible with '{}'.",
                    pipelineName,
                    passConfig.Name);
            }

            // Check that we have at least one compatible pipeline
            if (passConfig.CompatiblePipelines.empty())
            {
                VA_ENGINE_ERROR(
                    "[RenderGraph] RenderPass '{}' has no compatible pipeline.",
                    passConfig.Name);
                return false;
            }
        }

        return true;
    }

    std::vector<Resources::RenderPassPtr> RenderGraph::GetExecutionOrder()
    {
        std::vector<Resources::RenderPassPtr> executionOrder;
        std::unordered_set<UUID> visited;
        std::unordered_set<UUID> visiting; // For cycle detection

        // Simple topological sort based on dependencies
        std::function<bool(const UUID&)> visit = [&](const UUID& passUUID) -> bool
        {
            if (visiting.contains(passUUID))
            {
                auto* node = FindRenderPassNode(passUUID);
                VA_ENGINE_ERROR(
                    "[RenderGraph] Cycle detected involving pass '{}'.",
                    node ? node->Config.Name : "Unknown");
                return false;
            }

            if (visited.contains(passUUID))
            {
                return true;
            }

            visiting.insert(passUUID);

            // Find the node for this pass
            auto* node = FindRenderPassNode(passUUID);
            if (!node)
            {
                VA_ENGINE_ERROR(
                    "[RenderGraph] Failed to find RenderPass node f.",
                    static_cast<uint64_t>(passUUID));
                return false;
            }

            // Visit all dependencies first
            for (const auto& dependencyUUID : node->DependenciesUUIDs)
            {
                if (!visit(dependencyUUID))
                {
                    return false;
                }
            }

            visiting.erase(passUUID);
            visited.insert(passUUID);
            executionOrder.push_back(node->RenderPass);

            return true;
        };

        // Visit all passes
        for (const auto& uuid : m_RenderPassesNodes | std::views::keys)
        {
            if (!visit(uuid))
            {
                VA_ENGINE_ERROR("[RenderGraph] Failed to compute execution order.");
                return {};
            }
        }

        // Reverse the order since we built it backwards (dependencies first)
        std::ranges::reverse(executionOrder);

        return executionOrder;
    }

    void RenderGraph::RenderPassContent(
        Resources::RenderPassPtr pass,
        Resources::RenderTargetPtr target,
        const FrameData& frameData)
    {
        auto* passNode = FindRenderPassNode(pass->GetUUID());
        if (!passNode)
        {
            VA_ENGINE_ERROR(
                "[RenderGraph] Failed to find RenderPass node for pass '{}'.",
                pass->GetName());
            return;
        }

        const auto& passConfig = passNode->Config;

        if (passConfig.CompatiblePipelines.empty())
        {
            VA_ENGINE_WARN(
                "[RenderGraph] RenderPass '{}' has no compatible pipelines.",
                pass->GetName());
            return;
        }

        // For now, we will just take the first compatible pipeline
        // Later, we will select the best one based on some parameters.
        const auto& pipelineName = passConfig.CompatiblePipelines[0];

        auto signature = g_PipelineSystem->CreateSignatureFromPass(passConfig);
        auto pipeline = g_PipelineSystem->GetCachedPipeline(pipelineName, signature);

        if (!pipeline)
        {
            VA_ENGINE_ERROR(
                "[RenderGraph] No compiled pipeline '{}' found for pass '{}'.",
                pipelineName,
                pass->GetName());
            return;
        }

        pipeline->Bind(m_RHI);

        m_RHI.UpdateGlobalState(pipeline, frameData.Projection, frameData.View);

        switch (passConfig.Type)
        {
            case RenderPassType::ForwardOpaque:
            case RenderPassType::ForwardTransparent:
                RenderForwardPass(passConfig, pipeline, frameData);
                break;

            case RenderPassType::Shadow:
                RenderShadowPass(passConfig, pipeline, frameData);
                break;

            case RenderPassType::DepthPrepass:
                RenderDepthPrepassPass(passConfig, pipeline, frameData);
                break;

            case RenderPassType::PostProcess:
                RenderPostProcessPass(passConfig, pipeline, frameData);
                break;

            case RenderPassType::UI:
                RenderUIPass(passConfig, pipeline, frameData);
                break;

            default:
                VA_ENGINE_WARN("[RenderGraph] Unknown RenderPass type for '{}'.", pass->GetName());
                break;
        }
    }

    void RenderGraph::RenderForwardPass(
        const RenderPassConfig& passConfig,
        const Resources::PipelinePtr& pipeline,
        const FrameData& frameData)
    {
        // For now, just a simple test with a material
        // Later, we will use a real scene manager with ECS
        const auto& defaultMat = RenderCommand::s_TestMaterial
                                     ? RenderCommand::s_TestMaterial
                                     : g_MaterialSystem->GetDefaultMaterial();

        if (!defaultMat)
        {
            VA_ENGINE_ERROR("[RenderGraph] Failed to get default material.");
            return;
        }

        const auto geometry = Resources::GeometryRenderData(
            Math::Mat4::Identity(),
            defaultMat,
            RenderCommand::s_TestMesh);

        defaultMat->Bind(m_RHI, pipeline);
        m_RHI.DrawMesh(geometry, pipeline);
    }

    void RenderGraph::RenderShadowPass(
        const RenderPassConfig& passConfig,
        const Resources::PipelinePtr& pipeline,
        const FrameData& frameData)
    {
        // TODO: Implement shadow mapping
    }

    void RenderGraph::RenderDepthPrepassPass(
        const RenderPassConfig& passConfig,
        const Resources::PipelinePtr& pipeline,
        const FrameData& frameData)
    {
        // TODO: Implement depth prepass
    }

    void RenderGraph::RenderPostProcessPass(
        const RenderPassConfig& passConfig,
        const Resources::PipelinePtr& pipeline,
        const FrameData& frameData)
    {
        //TODO: Implement post process
    }

    void RenderGraph::RenderUIPass(
        const RenderPassConfig& passConfig,
        const Resources::PipelinePtr& pipeline,
        const FrameData& frameData)
    {
        //TODO: Implement UI rendering
    }
} // Renderer
// VoidArchitect
