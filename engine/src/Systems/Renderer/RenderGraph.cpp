//
// Created by Michael Desmedt on 02/06/2025.
//
#include "RenderGraph.hpp"

#include <ranges>

#include "Core/Logger.hpp"
#include "Platform/RHI/IRenderingHardware.hpp"
#include "Resources/RenderTarget.hpp"
#include "Systems/MaterialSystem.hpp"
#include "Systems/RenderStateSystem.hpp"

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

    UUID RenderGraph::AddRenderPass(const UUID& templateUUID, const std::string& instanceName)
    {
        if (!g_RenderPassSystem->HasRenderPassTemplate(templateUUID))
        {
            VA_ENGINE_ERROR(
                "[RenderGraph] RenderPass template UUID '{}' not found.",
                static_cast<uint64_t>(templateUUID));
            return InvalidUUID;
        }

        const UUID instanceUUID;
        RenderPassNode node{
            .instanceUUID = instanceUUID,
            .templateUUID = templateUUID,
            .instanceName = instanceName.empty()
                                ? ("Pass_" + std::to_string(static_cast<uint64_t>(instanceUUID)))
                                : instanceName,
            .RenderPass = nullptr
        };

        m_RenderPassesNodes[instanceUUID] = std::move(node);
        m_IsCompiled = false;

        VA_ENGINE_TRACE(
            "[RenderGraph] RenderPass '{}' added with UUID {}.",
            node.instanceName,
            static_cast<uint64_t>(instanceUUID));

        return instanceUUID;
    }

    UUID RenderGraph::AddRenderTarget(const RenderTargetConfig& config)
    {
        // Create and store node
        const UUID instanceUUID;
        RenderTargetNode node{
            .instanceUUID = instanceUUID,
            .Config = config,
            .RenderTarget = nullptr
        };

        m_RenderTargetsNodes[instanceUUID] = std::move(node);
        m_IsCompiled = false;

        VA_ENGINE_TRACE(
            "[RenderGraph] RenderTarget '{}' added with UUID {}.",
            config.Name,
            static_cast<uint64_t>(instanceUUID));

        return instanceUUID;
    }

    void RenderGraph::AddDependency(const UUID& fromUUID, const UUID& toUUID)
    {
        // Find the 'from' node
        auto* fromNode = FindRenderPassNode(fromUUID);
        if (!fromNode)
        {
            VA_ENGINE_ERROR(
                "[RenderGraph] Cannot find RenderPass node for UUID '{}' for dependency source.",
                static_cast<uint64_t>(fromUUID));
            return;
        }

        // Check if a dependency already exists
        auto& dependencies = fromNode->DependenciesUUIDs;
        auto* toNode = FindRenderPassNode(toUUID);
        if (std::ranges::find(dependencies, toUUID) != dependencies.end())
        {
            VA_ENGINE_WARN(
                "[RenderGraph] Dependency already exists between RenderPass '{}' -> '{}'.",
                fromNode->instanceName,
                toNode->instanceName);
            return;
        }

        dependencies.push_back(toUUID);

        VA_ENGINE_TRACE(
            "[RenderGraph] Dependency added between RenderPass '{}' -> '{}'.",
            fromNode->instanceName,
            toNode->instanceName);
    }

    void RenderGraph::ConnectPassToTarget(
        const UUID& passUUID,
        const UUID& targetUUID)
    {
        auto* passNode = FindRenderPassNode(passUUID);
        if (!passNode)
        {
            VA_ENGINE_ERROR(
                "[RenderGraph] Cannot find RenderPass node for UUID '{}' for target connection.",
                static_cast<uint64_t>(passUUID));
            return;
        }

        // Check if target already connected
        auto& outputs = passNode->OutputsUUIDs;
        auto* targetNode = FindRenderTargetNode(targetUUID);
        if (std::ranges::find(outputs, targetUUID) != outputs.end())
        {
            VA_ENGINE_WARN(
                "[RenderGraph] RenderPass '{}' already connected to RenderTarget '{}'.",
                passNode->instanceName,
                targetNode->Config.Name);
            return;
        }

        outputs.push_back(targetUUID);

        m_IsCompiled = false;

        VA_ENGINE_TRACE(
            "[RenderGraph] RenderPass '{}' connected to RenderTarget '{}'.",
            passNode->instanceName,
            targetNode->Config.Name);
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

        if (!ValidateConnections())
        {
            VA_ENGINE_ERROR("[RenderGraph] Cannot find a valid connection between passes.");
            return false;
        }

        if (!ValidateNoCycles())
        {
            VA_ENGINE_ERROR("[RenderGraph] Cannot find a valid execution order.");
            return false;
        }

        if (!ValidatePassRenderStateCompatibility())
        {
            VA_ENGINE_ERROR("[RenderGraph] Cannot find a compatible pipeline for passes.");
            return false;
        }

        VA_ENGINE_TRACE("[RenderGraph] Graph validated.");
        return true;
    }

    bool RenderGraph::ValidateConnections()
    {
        for (const auto& passNode : m_RenderPassesNodes | std::views::values)
        {
            // Validate that all passes have outputs
            if (passNode.OutputsUUIDs.empty())
            {
                VA_ENGINE_ERROR(
                    "[RenderGraph] RenderPass '{}' has no output.",
                    passNode.instanceName);
                return false;
            }

            // Validate that output targets are registered in the graph
            for (const auto& outputUUID : passNode.OutputsUUIDs)
            {
                if (!FindRenderTargetNode(outputUUID))
                {
                    VA_ENGINE_ERROR(
                        "[RenderGraph] RenderPass '{}' has invalid output target '{}'.",
                        passNode.instanceName,
                        static_cast<uint64_t>(outputUUID)
                    );
                    return false;
                }
            }

            // Validate that all dependencies are registered in the graph
            for (const auto& dependencyUUID : passNode.DependenciesUUIDs)
            {
                if (!FindRenderPassNode(dependencyUUID))
                {
                    VA_ENGINE_ERROR(
                        "[RenderGraph] RenderPass '{}' has invalid dependency '{}'.",
                        passNode.instanceName,
                        static_cast<uint64_t>(dependencyUUID)
                    );
                    return false;
                }
            }
        }

        return true;
    }

    bool RenderGraph::ValidateNoCycles()
    {
        VAHashSet<UUID> visited;
        VAHashSet<UUID> visiting;

        std::function<bool(const UUID&)> hasCycle = [&](const UUID& passUUID) -> bool
        {
            if (visiting.contains(passUUID))
            {
                auto* node = FindRenderPassNode(passUUID);
                VA_ENGINE_ERROR(
                    "[RenderGraph] Cycle detected involving pass '{}'.",
                    node ? node->instanceName : "Unknown");
                return false;
            }

            if (visited.contains(passUUID))
                return false;

            visiting.insert(passUUID);

            if (const auto* node = FindRenderPassNode(passUUID))
            {
                for (const auto& depUUID : node->DependenciesUUIDs)
                {
                    if (hasCycle(depUUID))
                        return true;
                }
            }

            visiting.erase(passUUID);
            visited.insert(passUUID);
            return false;
        };

        return std::ranges::none_of(m_RenderPassesNodes | std::views::keys, hasCycle);
    }

    bool RenderGraph::ValidatePassRenderStateCompatibility()
    {
        for (const auto& passNode : m_RenderPassesNodes | std::views::values)
        {
            const auto& passConfig = g_RenderPassSystem->GetRenderPassTemplate(
                passNode.templateUUID);

            if (passConfig.Type == RenderPassType::Unknown)
            {
                VA_ENGINE_WARN("[RenderGraph] RenderPass '{}' has unknown type.", passConfig.Name);
                return false;
            }

            // Check that all RenderStates exist and are compatible
            for (const auto& renderStateName : passConfig.CompatibleStates)
            {
                // For now, just log declared compatibility
                // Later, we will check with the PipelineSystem
                VA_ENGINE_DEBUG(
                    "[RenderGraph]   - RenderState '{}' is compatible with '{}'.",
                    renderStateName,
                    passConfig.Name);
            }

            // Check that we have at least one compatible pipeline
            if (passConfig.CompatibleStates.empty())
            {
                VA_ENGINE_ERROR(
                    "[RenderGraph] RenderPass '{}' has no compatible pipeline.",
                    passConfig.Name);
                return false;
            }
        }

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

        // Step 1: Compute final execution order
        m_ExecutionOrder = ComputeExecutionOrder();
        if (m_ExecutionOrder.empty())
        {
            VA_ENGINE_ERROR("[RenderGraph] Failed to compute execution order.");
            return false;
        }

        // Step 2: Create RenderTargets (RenderPasses might need them for compatibility checking)
        if (!CompileRenderTargets())
        {
            VA_ENGINE_ERROR("[RenderGraph] Failed to compile RenderTargets.");
            return false;
        }

        // Step 3: Create RenderPasses from templates
        if (!CompileRenderPasses())
        {
            VA_ENGINE_ERROR("[RenderGraph] Failed to compile RenderPasses.");
            return false;
        }

        // Step 4: Create RenderStates from templates
        if (!CompileRenderStates())
        {
            VA_ENGINE_ERROR("[RenderGraph] Failed to compile RenderStates.");
            return false;
        }

        if (!CompileMaterials())
        {
            VA_ENGINE_ERROR("[RenderGraph] Failed to compile materials.");
            return false;
        }

        // Step 5: Assign required states from renderers
        AssignRequiredStates();

        // Step 6: Optimize execution to minimize state changes
        OptimizeExecutionOrder();

        // Mark as compiled
        m_IsCompiled = true;

        LogOptimizationMetrics();
        return true;
    }

    bool RenderGraph::CompileRenderTargets()
    {
        VA_ENGINE_DEBUG("[RenderGraph] Compiling RenderTargets...");

        for (auto& targetNode : m_RenderTargetsNodes | std::views::values)
        {
            if (!targetNode.RenderTarget)
            {
                // Create RenderTarget
                const auto rawTarget = m_RHI.CreateRenderTarget(targetNode.Config);
                if (!rawTarget)
                {
                    VA_ENGINE_ERROR(
                        "[RenderGraph] Failed to create RenderTarget '{}'.",
                        targetNode.Config.Name);
                    return false;
                }

                targetNode.RenderTarget = Resources::RenderTargetPtr(
                    rawTarget,
                    [this](const Resources::IRenderTarget* target)
                    {
                        this->ReleaseRenderTarget(target);
                        delete target;
                    });

                VA_ENGINE_TRACE(
                    "[RenderGraph] RenderTarget '{}' compiled.",
                    targetNode.Config.Name);
            }
        }

        VA_ENGINE_DEBUG("[RenderGraph] RenderTargets compiled successfully.");
        return true;
    }

    bool RenderGraph::CompileRenderPasses()
    {
        // Analyse execution order to determine pass positions
        for (size_t i = 0; i < m_ExecutionOrder.size(); i++)
        {
            auto* passNode = FindRenderPassNode(m_ExecutionOrder[i]);
            if (!passNode)
            {
                VA_ENGINE_ERROR(
                    "[RenderGraph] Invalid pass in execution order, skipping pass.");
                continue;
            }

            PassPosition position;
            if (m_ExecutionOrder.size() == 1)
                position = PassPosition::Standalone;
            else if (i == 0)
                position = PassPosition::First;
            else if (i == m_ExecutionOrder.size() - 1)
                position = PassPosition::Last;
            else
                position = PassPosition::Middle;

            passNode->ComputedPosition = position;
        }

        VA_ENGINE_DEBUG("[RenderGraph] Compiling RenderPasses...");

        // This step makes the real RHI RenderPass objects.
        for (auto& passNode : m_RenderPassesNodes | std::views::values)
        {
            if (!passNode.RenderPass)
            {
                passNode.RenderPass = g_RenderPassSystem->CreateRenderPass(
                    passNode.templateUUID,
                    passNode.ComputedPosition);

                if (!passNode.RenderPass)
                {
                    VA_ENGINE_ERROR(
                        "[RenderGraph] Failed to create RenderPass from template UUID {}.",
                        static_cast<uint64_t>(passNode.templateUUID));
                    return false;
                }

                VA_ENGINE_TRACE(
                    "[RenderGraph] RenderPass '{}' compiled.",
                    passNode.RenderPass->GetName());
            }
        }

        VA_ENGINE_DEBUG("[RenderGraph] RenderPasses compiled successfully.");
        return true;
    }

    bool RenderGraph::CompileRenderStates()
    {
        VA_ENGINE_DEBUG("[RenderGraph] Compiling RenderStates...");

        for (const auto& passNode : m_RenderPassesNodes | std::views::values)
        {
            const auto& passConfig = g_RenderPassSystem->GetRenderPassTemplate(
                passNode.templateUUID);
            const auto& renderPass = passNode.RenderPass;

            // For each compatible render state with this pass
            for (const auto& renderStateName : passConfig.CompatibleStates)
            {
                // Check that the template exists
                if (!g_RenderStateSystem->HasRenderStateTemplate(renderStateName))
                {
                    VA_ENGINE_ERROR(
                        "[RenderGraph] RenderState template '{}' not found for pass '{}'.",
                        renderStateName,
                        passConfig.Name);
                    return false;
                }

                // Create the renderState for this pass
                const auto renderState =
                    g_RenderStateSystem->CreateRenderState(renderStateName, passConfig, renderPass);
                if (!renderState)
                {
                    VA_ENGINE_ERROR(
                        "[RenderGraph] Failed to create RenderState for pass '{}'.",
                        passConfig.Name);
                    return false;
                }

                VA_ENGINE_TRACE(
                    "[RenderGraph] RenderState '{}' compiled for pass '{}'.",
                    renderStateName,
                    passConfig.Name);
            }
        }

        VA_ENGINE_DEBUG("[RenderGraph] RenderStates compiled successfully.");
        return true;
    }

    bool RenderGraph::CompileMaterials()
    {
        VA_ENGINE_DEBUG("[RenderGraph] Compiling materials...");

        for (const auto& request : m_MaterialRequests)
        {
            // Find pass mapping
            auto mappingIt = std::ranges::find_if(
                m_PassMappings,
                [&request](const PassMapping& mapping)
                {
                    return mapping.passType == request.passType;
                });

            if (mappingIt == m_PassMappings.end())
            {
                VA_ENGINE_ERROR(
                    "[RenderGraph] No pass mapping found for pass type '{}' for material '{}'.",
                    RenderPassTypeToString(request.passType),
                    request.identifier);
                return false;
            }

            const auto& mapping = *mappingIt;

            // Get the RenderState for this material
            auto renderState = g_RenderStateSystem->GetCachedRenderState(
                request.renderStateTemplate,
                mapping.signature);

            if (!renderState)
            {
                VA_ENGINE_ERROR(
                    "[RenderGraph] Failed to get RenderState '{}' for material '{}'.",
                    request.renderStateTemplate,
                    request.templateName);
                return false;
            }

            // Create material using MaterialSystem
            auto material = g_MaterialSystem->CreateMaterial(
                request.templateName,
                request.passType,
                renderState);
            if (!material)
            {
                VA_ENGINE_ERROR(
                    "[RenderGraph] Failed to create material '{}' for material request '{}'.",
                    request.templateName,
                    request.templateName);
                return false;
            }

            m_CompiledMaterials[request.templateName] = material;
            VA_ENGINE_TRACE(
                "[RenderGraph] Material '{}' compiled successfully.",
                request.templateName);
        }

        VA_ENGINE_DEBUG("[RenderGraph] Materials compiled successfully.");
        return true;
    }

    void RenderGraph::AssignRequiredStates()
    {
        VA_ENGINE_DEBUG("[RenderGraph] Assigning required states from renderers...");

        for (auto& passNode : m_RenderPassesNodes | std::views::values)
        {
            const auto& passConfig = g_RenderPassSystem->GetRenderPassTemplate(
                passNode.templateUUID);
            const auto* renderer = g_RenderPassSystem->GetPassRenderer(passConfig.Type);
            if (!renderer)
            {
                VA_ENGINE_ERROR(
                    "[RenderGraph] No renderer found for pass '{}' (Type: {}), skipping.",
                    passNode.instanceName,
                    RenderPassTypeToString(passConfig.Type));
                continue;
            }

            // Get the required state from the renderer
            passNode.RequiredStateTemplateName = renderer->GetCompatibleRenderState();

            if (passNode.RequiredStateTemplateName.empty())
            {
                VA_ENGINE_ERROR(
                    "[RenderGraph] No required RenderState for pass '{}', skipping.",
                    passNode.instanceName);
                continue;
            }

            // Verify the template exists
            if (!g_RenderStateSystem->HasRenderStateTemplate(passNode.RequiredStateTemplateName))
            {
                VA_ENGINE_ERROR(
                    "[RenderGraph] Required RenderState template '{}' not found for pass '{}', skipping.",
                    passNode.RequiredStateTemplateName,
                    passConfig.Name);
                continue;
            }

            // Create signature from the associated RenderPass
            const auto signature = g_RenderStateSystem->CreateSignatureFromPass(passConfig);

            // Get the required render state
            passNode.AssignedState = g_RenderStateSystem->GetCachedRenderState(
                passNode.RequiredStateTemplateName,
                signature);
            if (!passNode.AssignedState)
            {
                VA_ENGINE_ERROR(
                    "[RenderGraph] Failed to get RenderState for pass '{}', skipping.",
                    passNode.instanceName);
                continue;
            }

            VA_ENGINE_TRACE(
                "[RenderGraph] RenderState '{}' assigned to pass '{}'.",
                passNode.RequiredStateTemplateName,
                passConfig.Name);
        }
    }

    void RenderGraph::OptimizeExecutionOrder()
    {
        VA_ENGINE_DEBUG("[RenderGraph] Optimizing execution order...");

        // NOTE: For now, we will just do a simple optimization
        //      Grouping passes by required state when dependencies allow it
        //      This is a placeholder for now - we'll implement more sophisticated optimizations later
        //      when we have multiple RenderStates.
        VAHashMap<Resources::RenderStatePtr, VAArray<std::string>> stateGroups;

        for (const auto& passNode : m_RenderPassesNodes | std::views::values)
        {
            if (passNode.AssignedState)
            {
                stateGroups[passNode.AssignedState].push_back(passNode.instanceName);
            }
        }

        VA_ENGINE_DEBUG("[RenderGraph] Found {} unique render states:", stateGroups.size());
        for (const auto& [state, pass] : stateGroups)
        {
            VA_ENGINE_DEBUG(
                "[RenderGraph]   - State '{}' used by {} passes",
                state->GetName(),
                pass.size());
            for (const auto& passName : pass)
            {
                VA_ENGINE_TRACE(
                    "[RenderGraph]     - {}",
                    passName);
            }
        }
    }

    float RenderGraph::CalculateStateSwitchCost() const
    {
        if (m_ExecutionOrder.empty()) return 0.0f;

        uint32_t stateChanges = 0;
        Resources::RenderStatePtr lastState = nullptr;

        for (const auto& passUUID : m_ExecutionOrder)
        {
            const auto* passNode = FindRenderPassNode(passUUID);
            if (!passNode || !passNode->RenderPass)
            {
                VA_ENGINE_ERROR(
                    "[RenderGraph] Invalid pass in execution order, skipping pass.");
                continue;
            }

            if (lastState && lastState != passNode->AssignedState)
            {
                stateChanges++;
            }
            lastState = passNode->AssignedState;
        }

        return static_cast<float>(stateChanges) / static_cast<float>(m_ExecutionOrder.size());
    }

    VAArray<UUID> RenderGraph::ComputeExecutionOrder()
    {
        VAArray<UUID> executionOrder;
        VAHashSet<UUID> visited;
        VAHashSet<UUID> visiting; // For cycle detection

        // Simple topological sort based on dependencies
        std::function<bool(const UUID&)> visit = [&](const UUID& passUUID) -> bool
        {
            if (visiting.contains(passUUID))
            {
                auto* node = FindRenderPassNode(passUUID);
                VA_ENGINE_ERROR(
                    "[RenderGraph] Cycle detected involving pass '{}'.",
                    node ? node->instanceName : "Unknown");
                return false;
            }

            if (visited.contains(passUUID))
            {
                return true; // Already visited, skip
            }

            visiting.insert(passUUID);

            // Find the node for this pass
            if (const auto* node = FindRenderPassNode(passUUID))
            {
                // Visit all dependencies first
                for (const auto& dependencyUUID : node->DependenciesUUIDs)
                {
                    if (!visit(dependencyUUID))
                    {
                        return false;
                    }
                }
            }
            else
            {
                VA_ENGINE_ERROR(
                    "[RenderGraph] Failed to find RenderPass UUID {}.",
                    static_cast<uint64_t>(passUUID));
            }

            visiting.erase(passUUID);
            visited.insert(passUUID);
            executionOrder.push_back(passUUID);

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

    void RenderGraph::LogOptimizationMetrics() const
    {
        const float stateSwitchRatio = CalculateStateSwitchCost();

        VA_ENGINE_INFO(
            "[RenderGraph] Compilation complete. {} passes, {:.1f}% potential state switch ratio.",
            m_ExecutionOrder.size(),
            stateSwitchRatio * 100.0f);

        // Log execution order with state information
        VA_ENGINE_INFO("[RenderGraph] Execution order:");
        Resources::RenderStatePtr lastState = nullptr;
        for (const auto& passUUID : m_ExecutionOrder)
        {
            if (const auto* passNode = FindRenderPassNode(passUUID))
            {
                const bool stateChange = (lastState != passNode->AssignedState);
                VA_ENGINE_INFO(
                    "[RenderGraph]   - {} ({}): {} -> {}",
                    passNode->instanceName,
                    stateChange ? "State change" : "No state change",
                    passNode->AssignedState ? passNode->AssignedState->GetName() : "None",
                    passNode->RequiredStateTemplateName.empty()? passNode->RequiredStateTemplateName
                    : "None");
                lastState = passNode->AssignedState;
            }
        }
    }

    void RenderGraph::Execute(const FrameData& frameData)
    {
        if (!m_IsCompiled)
        {
            VA_ENGINE_ERROR("[RenderGraph] Graph is not compiled, cannot execute.");
            return;
        }

        // Execute passes in order (using cached execution order for performance)
        for (const auto& passUUID : m_ExecutionOrder)
        {
            auto* passNode = FindRenderPassNode(passUUID);
            if (!passNode || !passNode->RenderPass)
            {
                VA_ENGINE_ERROR(
                    "[RenderGraph] Invalid pass in execution order, skipping pass.");
                continue;
            }

            if (passNode->OutputsUUIDs.empty())
            {
                VA_ENGINE_ERROR(
                    "[RenderGraph] RenderPass '{}' has no output, skipping pass.",
                    passNode->instanceName);
                continue;
            }

            // Get first target (assume single output for now)
            auto targetUUID = passNode->OutputsUUIDs[0];
            const auto* targetNode = FindRenderTargetNode(targetUUID);
            if (!targetNode || !targetNode->RenderTarget)
            {
                VA_ENGINE_ERROR(
                    "[RenderGraph] Invalid target for pass '{}', skipping pass.",
                    passNode->instanceName);
                continue;
            }

            // Execute the pass
            const auto& renderPass = passNode->RenderPass;
            auto& renderTarget = targetNode->RenderTarget;
            renderPass->Begin(m_RHI, renderTarget);
            RenderPassContent(renderPass, renderTarget, frameData);
            renderPass->End(m_RHI);
        }

        m_LastBoundState = nullptr;
        m_StateChangeCount = 0;
    }

    void RenderGraph::RegisterPassMapping(
        RenderPassType passType,
        const std::string& renderPassName,
        const RenderStateSignature& signature)
    {
        m_PassMappings.push_back({passType, renderPassName, signature});
        VA_ENGINE_TRACE(
            "[RenderGraph] Registered pass mapping: {} -> {}.",
            RenderPassTypeToString(passType),
            renderPassName);
    }

    void RenderGraph::RequestMaterialForPassType(
        const std::string& identifier,
        const std::string& templateName,
        const std::string& renderStateTemplate,
        RenderPassType passType)
    {
        m_MaterialRequests.push_back({templateName, renderStateTemplate, identifier, passType});
        VA_ENGINE_TRACE(
            "[RenderGraph] Requested material for pass type '{}': {} -> {}.",
            RenderPassTypeToString(passType),
            templateName,
            renderStateTemplate);
    }

    void RenderGraph::BindRenderStateIfNeeded(const Resources::RenderStatePtr& newState)
    {
        if (!newState)
        {
            VA_ENGINE_WARN("[RenderGraph] Attempting to bind a null RenderState.");
            return;
        }

        if (m_LastBoundState != newState)
        {
            newState->Bind(m_RHI);
            m_LastBoundState = newState;
            m_StateChangeCount++;

            // VA_ENGINE_TRACE(
            //     "[RenderGraph] RenderState '{}' bound, total changes: {}.",
            //     newState->GetName(),
            //     m_StateChangeCount);
        }
    }

    Resources::MaterialPtr RenderGraph::GetMaterial(const std::string& identifier) const
    {
        auto it = m_CompiledMaterials.find(identifier);
        return (it != m_CompiledMaterials.end()) ? it->second : nullptr;
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
                // Update config for consistency
                node.Config.Width = width;
                node.Config.Height = height;

                if (node.RenderTarget)
                {
                    node.RenderTarget->Resize(width, height);
                }

                VA_ENGINE_TRACE("[RenderGraph] Resized main RenderTarget '{}'.", node.Config.Name);
            }
        }

        m_IsCompiled = false;
    }

    RenderGraph::RenderPassNode* RenderGraph::FindRenderPassNode(const UUID& instanceUUID)
    {
        const auto it = m_RenderPassesNodes.find(instanceUUID);
        return (it != m_RenderPassesNodes.end()) ? &it->second : nullptr;
    }

    const RenderGraph::RenderPassNode* RenderGraph::FindRenderPassNode(
        const UUID& instanceUUID) const
    {
        const auto it = m_RenderPassesNodes.find(instanceUUID);
        return (it != m_RenderPassesNodes.end()) ? &it->second : nullptr;
    }

    RenderGraph::RenderTargetNode* RenderGraph::FindRenderTargetNode(const UUID& instanceUUID)
    {
        const auto it = m_RenderTargetsNodes.find(instanceUUID);
        return (it != m_RenderTargetsNodes.end()) ? &it->second : nullptr;
    }

    const RenderGraph::RenderTargetNode* RenderGraph::FindRenderTargetNode(
        const UUID& instanceUUID) const
    {
        const auto it = m_RenderTargetsNodes.find(instanceUUID);
        return (it != m_RenderTargetsNodes.end()) ? &it->second : nullptr;
    }

    RenderGraph::RenderPassNode* RenderGraph::FindRenderPassNode(Resources::RenderPassPtr& pass)
    {
        if (!pass)
            return nullptr;

        for (auto& node : m_RenderPassesNodes | std::views::values)
            if (node.RenderPass == pass)
                return &node;

        return nullptr;
    }

    RenderGraph::RenderTargetNode* RenderGraph::FindRenderTargetNode(
        Resources::RenderTargetPtr& target)
    {
        if (!target)
            return nullptr;

        for (auto& node : m_RenderTargetsNodes | std::views::values)
            if (node.RenderTarget == target)
                return &node;

        return nullptr;
    }

    const RenderGraph::RenderPassNode* RenderGraph::FindRenderPassNode(
        const Resources::RenderPassPtr& pass) const
    {
        if (!pass)
            return nullptr;

        for (const auto& node : m_RenderPassesNodes | std::views::values)
            if (node.RenderPass == pass)
                return &node;

        return nullptr;
    }

    const RenderGraph::RenderTargetNode* RenderGraph::FindRenderTargetNode(
        const Resources::RenderTargetPtr& target) const
    {
        if (!target)
            return nullptr;

        for (const auto& node : m_RenderTargetsNodes | std::views::values)
            if (node.RenderTarget == target)
                return &node;

        return nullptr;
    }

    void RenderGraph::ReleaseRenderPass(const Resources::IRenderPass* pass)
    {
        if (!pass || m_IsDestroying)
            return;

        const auto passUUID = pass->GetUUID();

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
        if (!target || m_IsDestroying)
            return;

        const auto targetUUID = target->GetUUID();

        if (const auto it = m_RenderTargetsNodes.find(targetUUID); it != m_RenderTargetsNodes.end())
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

    void RenderGraph::RenderPassContent(
        const Resources::RenderPassPtr& pass,
        const Resources::RenderTargetPtr& target,
        const FrameData& frameData)
    {
        const auto* passNode = FindRenderPassNode(pass);
        if (!passNode)
        {
            VA_ENGINE_ERROR(
                "[RenderGraph] Failed to find RenderPass node for pass '{}'.",
                pass->GetName());
            return;
        }

        const auto& passConfig = g_RenderPassSystem->GetRenderPassTemplate(passNode->templateUUID);
        auto* passRenderer = g_RenderPassSystem->GetPassRenderer(passConfig.Type);
        if (!passRenderer)
        {
            VA_ENGINE_ERROR(
                "[RenderGraph] Failed to get pass renderer for pass '{}'.",
                RenderPassTypeToString(passConfig.Type));
            return;
        }

        if (passNode->AssignedState)
        {
            BindRenderStateIfNeeded(passNode->AssignedState);

            //Bind the global state to this RenderState
            m_RHI.BindGlobalState(passNode->AssignedState);
        }
        else
        {
            VA_ENGINE_ERROR("[RenderGraph] No RenderState assigned to pass '{}'.", pass->GetName());
            return;
        }

        const RenderContext context{
            .Rhi = m_RHI,
            .FrameData = frameData,
            .RenderPass = pass,
            .RenderTarget = target,
            .RenderState = passNode->AssignedState
        };

        passRenderer->Execute(context);
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

        const auto mainTargetUUID = AddRenderTarget(mainTargetConfig);

        // === 2. Register Forward Render Pass ===
        const auto forwardPassTemplateUUID = g_RenderPassSystem->
            GetRenderPassTemplateUUID("ForwardOpaque");
        const auto forwardPassUUID = AddRenderPass(forwardPassTemplateUUID);

        // === 3. Register UI Render Pass ===
        const auto uiPassTemplateUUID = g_RenderPassSystem->GetRenderPassTemplateUUID("UI");
        const auto uiPassUUID = AddRenderPass(uiPassTemplateUUID);

        // === 4. Link Render Passes ===
        // Forward pass -> UI pass
        AddDependency(forwardPassUUID, uiPassUUID);

        // === 5. Connect Pass to Target ===
        ConnectPassToTarget(forwardPassUUID, mainTargetUUID);
        ConnectPassToTarget(uiPassUUID, mainTargetUUID);

        // === 6. Register materials ===
        RegisterPassMapping(
            RenderPassType::ForwardOpaque,
            "ForwardOpaque",
            g_RenderStateSystem->CreateSignatureFromPass(
                g_RenderPassSystem->GetRenderPassTemplate(forwardPassTemplateUUID)));

        RegisterPassMapping(
            RenderPassType::UI,
            "UI",
            g_RenderStateSystem->CreateSignatureFromPass(
                g_RenderPassSystem->GetRenderPassTemplate(uiPassTemplateUUID)));

        RequestMaterialForPassType(
            "Material",
            "TestMaterial",
            "Default",
            RenderPassType::ForwardOpaque);
        RequestMaterialForPassType("UI", "DefaultUI", "UI", RenderPassType::UI),

            VA_ENGINE_INFO("[RenderGraph] Forward Renderer setup complete, ready for compilation.");
    }
} // namespace VoidArchitect::Renderer
// VoidArchitect
