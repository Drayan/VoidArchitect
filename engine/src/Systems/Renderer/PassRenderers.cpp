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

    // =============================================================================================
    // ForwardOpaquePassRenderer Implementation
    // =============================================================================================

    void ForwardOpaquePassRenderer::Execute(
        const RenderContext& context)
    {
        if (!context.RenderState)
        {
            VA_ENGINE_ERROR(
                "[ForwardOpaquePassRenderer] No render state provided by the RenderGraph.");
            return;
        }

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

        defaultMat->Bind(context.Rhi, context.RenderState);
        context.Rhi.DrawMesh(geometry, context.RenderState);
    }

    std::string ForwardOpaquePassRenderer::GetCompatibleRenderState() const
    {
        return "Default";
    }

    bool ForwardOpaquePassRenderer::IsCompatibleWith(const RenderPassType passType) const
    {
        return passType == RenderPassType::ForwardOpaque;
    }
}
