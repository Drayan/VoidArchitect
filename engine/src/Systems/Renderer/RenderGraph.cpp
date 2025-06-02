//
// Created by Michael Desmedt on 02/06/2025.
//
#include "RenderGraph.hpp"

#include <ranges>

#include "Core/Logger.hpp"
#include "Platform/RHI/IRenderingHardware.hpp"
#include "Resources/RenderTarget.hpp"
#include "Systems/PipelineSystem.hpp"

namespace VoidArchitect
{
    namespace Renderer
    {
        RenderGraph::RenderGraph(Platform::IRenderingHardware& rhi)
            : m_RHI(rhi)
        {
            m_RenderPassesNodes.reserve(16);
            m_RenderTargetsNodes.reserve(8);
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
            Resources::RenderPassPtr pass,
            Resources::RenderTargetPtr target)
        {
            if (!pass || !target)
            {
                VA_ENGINE_ERROR(
                    "[RenderGraph] Cannot connect RenderPass to RenderTarget, at least one of them is invalid.");
                return;
            }

            auto passUUID = pass->GetUUID();
            auto targetUUID = target->GetUUID();

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
            for (const auto& [uuid, passNode] : m_RenderPassesNodes)
            {
                if (passNode.OutputsUUIDs.empty())
                {
                    VA_ENGINE_ERROR(
                        "[RenderGraph] RenderPass '{}' has no output.",
                        passNode.Config.Name);
                    return false;
                }
            }

            // TODO: Add more validation as needed
            //   - ValidateNoCycles()
            //   - ValidateReferences()
            //   - Validate attachement compatibility

            VA_ENGINE_TRACE("[RenderGraph] Graph validated.");
            return true;
        }

        void RenderGraph::Compile()
        {
            VA_ENGINE_INFO("[RenderGraph] Compiling graph...");

            // Validate the graph
            if (!Validate())
            {
                VA_ENGINE_ERROR("[RenderGraph] Graph validation failed, cannot compile.");
                return;
            }

            // Compute execution order
            m_ExecutionOrder = GetExecutionOrder();

            if (m_ExecutionOrder.empty())
            {
                VA_ENGINE_ERROR("[RenderGraph] Failed to compute execution order.");
                return;
            }

            // Mark as compiled
            m_IsCompiled = true;

            VA_ENGINE_INFO("[RenderGraph] Graph compiled successfully. Execution order:");
            for (size_t i = 0; i < m_ExecutionOrder.size(); ++i)
            {
                VA_ENGINE_INFO("[RenderGraph]   {}: '{}'", i, m_ExecutionOrder[i]->GetName());
            }
        }

        void RenderGraph::Execute(const FrameData& frameData)
        {
            if (!m_IsCompiled)
            {
                VA_ENGINE_ERROR("[RenderGraph] Graph is not compiled, cannot execute.");
                return;
            }

            VA_ENGINE_TRACE(
                "[RenderGraph] Executing render graph with {} passes.",
                m_ExecutionOrder.size());

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

                VA_ENGINE_TRACE(
                    "[RenderGraph] Executing pass '{}' -> target '{}'.",
                    pass->GetName(),
                    renderTarget->GetName());

                pass->Begin(m_RHI, renderTarget);
                RenderPassContent(pass, renderTarget, frameData);
                pass->End(m_RHI);
            }

            VA_ENGINE_TRACE("[RenderGraph] Render graph executed successfully.");
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
            VA_ENGINE_INFO("[RenderGraph Setting up Forward Renderer ({}x{})]", width, height);

            // Store current dimensions
            m_CurrentWidth = width;
            m_CurrentHeight = height;

            // === 1. Create Main Reder Target (swapchain) ===
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

            // Color attachment (swapchain)
            RenderPassConfig::AttachmentConfig colorAttachment;
            colorAttachment.Name = "color";
            colorAttachment.Format = TextureFormat::SWAPCHAIN_FORMAT;
            colorAttachment.LoadOp = LoadOp::Clear;
            colorAttachment.StoreOp = StoreOp::Store;
            colorAttachment.ClearColor = Math::Vec4(0.1f, 0.1f, 0.1f, 1.0f);

            // Depth attachment
            RenderPassConfig::AttachmentConfig depthAttachment;
            depthAttachment.Name = "depth";
            depthAttachment.Format = TextureFormat::SWAPCHAIN_DEPTH;
            depthAttachment.LoadOp = LoadOp::Clear;
            depthAttachment.StoreOp = StoreOp::DontCare;
            depthAttachment.ClearDepth = 1.0f;
            depthAttachment.ClearStencil = 0;

            forwardPassConfig.Attachments = {colorAttachment, depthAttachment};

            // Single subpass with color + depth
            RenderPassConfig::SubpassConfig subpass;
            subpass.Name = "main";
            subpass.ColorAttachments = {"color"};
            subpass.DepthAttachment = "depth";

            forwardPassConfig.Subpasses = {subpass};

            auto forwardPass = AddRenderPass(forwardPassConfig);
            if (!forwardPass)
            {
                VA_ENGINE_CRITICAL("[RenderGraph] Failed to create Forward RenderPass.");
                return;
            }

            // === 3. Connect Pass to Target ===
            ConnectPassToTarget(forwardPass, mainTarget);

            VA_ENGINE_INFO("[RenderGraph] Forwar Renderer setup compelte.");
            VA_ENGINE_INFO(
                "[RenderGraph]   - Main Target: '{}' ({}x{})",
                mainTarget->GetName(),
                width,
                height);
            VA_ENGINE_INFO(
                "[RenderGraph]   - Forward Pass: '{}' with {} attachments",
                forwardPass->GetName(),
                forwardPassConfig.Attachments.size());
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
            if (!pass) return;

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
            if (!target) return;

            auto targetUUID = target->GetUUID();

            m_RenderTargetsNodes.erase(targetUUID);

            // Remove from all pass outputs
            for (auto& node : m_RenderPassesNodes | std::views::values)
            {
                auto& outputs = node.OutputsUUIDs;
                std::erase(outputs, targetUUID);
            }

            m_IsCompiled = false;
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
            // This is where specific pass rendering would happen
            if (pass->GetName() == "ForwardPass")
            {
                //Bind the default pipeline for this pass
                //TODO : Each pass should specify its own pipeline in the future
                auto pipeline = g_PipelineSystem->GetDefaultPipeline();
                if (pipeline)
                {
                    pipeline->Bind(m_RHI);
                }

                // Update the global state
                m_RHI.UpdateGlobalState(pipeline, frameData.Projection, frameData.View);

                // TODO: This is where we'd render the actual geometry
                //  For now, we'll let RenderCommand continue to handle the test geometry
                //  In the future, this would iterate through a scene (ECS) and render all the entities
                //  with a RenderComponent

                VA_ENGINE_TRACE("[RenderGraph] ForwardPass content rendered.");
            }
            else
            {
                VA_ENGINE_WARN(
                    "[RenderGraph] Unknown RenderPass '{}', skipping content rendering.",
                    pass->GetName());
            }
        }
    } // Renderer
} // VoidArchitect
