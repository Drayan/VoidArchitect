//
// Created by Michael Desmedt on 04/06/2025.
//
#include "PassRenderers.hpp"

#include "RenderCommand.hpp"
#include "RenderGraph.hpp"
#include "Core/Logger.hpp"
#include "Platform/RHI/IRenderingHardware.hpp"
#include "Systems/MaterialSystem.hpp"
#include "Systems/RenderPassSystem.hpp"
#include "Systems/RenderStateSystem.hpp"

namespace VoidArchitect::Renderer
{
    const std::string ForwardOpaquePassRenderer::m_Name = "ForwardOpaquePassRenderer";

    void ForwardOpaquePassRenderer::Execute(const RenderContext& context)
    {
        // Get the RenderState needed
        // NOTE : We'll need to access the RenderGraph through the context or pass it differently
        //        but for now, we'll get the RenderState information directly from the system.
        const auto* passNode = g_RenderGraph->FindRenderPassNode(context.RenderPass);
        if (!passNode)
        {
            VA_ENGINE_WARN("[ForwardOpaquePassRenderer] No pass node found.");
            return;
        }
        const auto& config = g_RenderPassSystem->GetRenderPassTemplate(passNode->templateUUID);
        if (config.CompatibleStates.empty())
        {
            VA_ENGINE_WARN("[ForwardOpaquePassRenderer] No compatible render state found.");
            return;
        }

        const auto& renderStateName = config.CompatibleStates[0];
        const auto signature = g_RenderStateSystem->CreateSignatureFromPass(config);
        const auto renderState = g_RenderStateSystem->GetCachedRenderState(
            renderStateName,
            signature);

        if (!renderState)
        {
            VA_ENGINE_ERROR(
                "[ForwardOpaquePassRenderer] Failed to get render state '{}'.",
                renderStateName);
            return;
        }

        // Bind the state
        renderState->Bind(context.Rhi);
        context.Rhi.UpdateGlobalState(
            renderState,
            context.FrameData.Projection,
            context.FrameData.View);

        // Render test geometry
        const auto& defaultMat = RenderCommand::s_TestMaterial
                                     ? RenderCommand::s_TestMaterial
                                     : g_MaterialSystem->GetDefaultMaterial();

        if (!defaultMat)
        {
            VA_ENGINE_WARN("[ForwardOpaquePassRenderer] No default material found.");
            return;
        }

        const auto geometry = Resources::GeometryRenderData(
            Math::Mat4::Identity(),
            defaultMat,
            RenderCommand::s_TestMesh);

        defaultMat->Bind(context.Rhi, renderState);
        context.Rhi.DrawMesh(geometry, renderState);
    }

    bool ForwardOpaquePassRenderer::IsCompatibleWith(const RenderPassType passType) const
    {
        return passType == RenderPassType::ForwardOpaque;
    }
}
