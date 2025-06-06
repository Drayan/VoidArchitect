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

namespace VoidArchitect::Renderer
{
    const std::string ForwardOpaquePassRenderer::m_Name = "ForwardOpaquePassRenderer";
    const std::string UIPassRenderer::m_Name = "UIPassRenderer";

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
        auto defaultMat = g_MaterialSystem->GetCachedMaterial(
            "TestMaterial",
            RenderPassType::ForwardOpaque,
            context.RenderState->GetUUID());

        if (!defaultMat)
        {
            VA_ENGINE_WARN("[ForwardOpaquePassRenderer] No default material found.");
            return;
        }

        static float angle = 0.f;
        angle += (0.5f * context.FrameData.deltaTime);
        const auto geometry = Resources::GeometryRenderData(
            Math::Mat4::Rotate(angle, Math::Vec3::Up()),
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

    //==============================================================================================
    // UIPassRenderer Implementation
    //==============================================================================================

    void UIPassRenderer::Execute(const RenderContext& context)
    {
        if (!context.RenderState)
        {
            VA_ENGINE_ERROR("[UIPassRenderer] No render state provided by the RenderGraph.");
            return;
        }

        // Create a simple UI quad in normalized coordinates (-1 to +1)
        auto uiMesh = RenderCommand::s_UIMesh;
        if (!uiMesh)
        {
            VA_ENGINE_ERROR("[UIPassRenderer] Failed to create UI mesh.");
            return;
        }

        // Use default material for now
        auto uiMaterial = g_MaterialSystem->GetCachedMaterial(
            "DefaultUI",
            RenderPassType::UI,
            context.RenderState->GetUUID());
        if (!uiMaterial)
        {
            VA_ENGINE_ERROR("[UIPassRenderer] Failed to get default material.");
            return;
        }

        // Create geometry render data
        const auto uiGeometry = Resources::GeometryRenderData(
            Math::Mat4::Translate(0.15f * 0.5f, 0.15f * 0.5f, 0.f),
            uiMaterial,
            uiMesh);

        // Render the UI quad
        uiMaterial->Bind(context.Rhi, context.RenderState);
        context.Rhi.DrawMesh(uiGeometry, context.RenderState);
    }

    std::string UIPassRenderer::GetCompatibleRenderState() const
    {
        return "UI";
    }

    bool UIPassRenderer::IsCompatibleWith(const RenderPassType passType) const
    {
        return passType == RenderPassType::UI;
    }
}
