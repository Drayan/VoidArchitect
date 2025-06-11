//
// Created by Michael Desmedt on 04/06/2025.
//
#include "PassRenderers.hpp"

#include "RenderCommand.hpp"
#include "RenderGraph.hpp"
#include "RenderGraphBuilder.hpp"
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

    void ForwardOpaquePassRenderer::Setup(RenderGraphBuilder& builder)
    {
        builder.ReadsFrom("TestMaterial").WritesToColorBuffer().WritesToDepthBuffer();
    }

    Renderer::RenderPassConfig ForwardOpaquePassRenderer::GetRenderPassConfig() const
    {
        RenderPassConfig config;
        config.attachments = {
            {
                "color",
                TextureFormat::SWAPCHAIN_FORMAT,
                LoadOp::Clear,
                StoreOp::Store,
                Math::Vec4::One()
            },
            {"depth", TextureFormat::SWAPCHAIN_DEPTH, LoadOp::Clear, StoreOp::DontCare}
        };
        config.type = RenderPassType::ForwardOpaque;
        config.name = m_Name;
        return config;
    }

    void ForwardOpaquePassRenderer::Execute(const RenderContext& context)
    {
        // if (!context.RenderState)
        // {
        //     VA_ENGINE_ERROR(
        //         "[ForwardOpaquePassRenderer] No render state provided by the RenderGraph.");
        //     return;
        // }

        const auto testMat = g_MaterialSystem->GetHandleFor("TestMaterial");
        if (testMat == InvalidMaterialHandle)
        {
            VA_ENGINE_ERROR("[ForwardOpaquePassRenderer] Failed to get test material.");
            return;
        }

        // Render test geometry
        // auto defaultMat = g_MaterialSystem->GetCachedMaterial(
        //     "TestMaterial",
        //     RenderPassType::ForwardOpaque,
        //     context.RenderState->GetUUID());
        //
        // if (!defaultMat)
        // {
        //     VA_ENGINE_WARN("[ForwardOpaquePassRenderer] No default material found.");
        //     return;
        // }
        //
        // static float angle = 0.f;
        // angle += (0.5f * context.FrameData.deltaTime);
        // const auto geometry = Resources::GeometryRenderData(
        //     Math::Mat4::Rotate(angle, Math::Vec3::Up()),
        //     defaultMat,
        //     RenderCommand::s_TestMesh);
        //
        // defaultMat->Bind(context.Rhi, context.RenderState);
        // context.Rhi.DrawMesh(geometry, context.RenderState);
    }

    //==============================================================================================
    // UIPassRenderer Implementation
    //==============================================================================================

    void UIPassRenderer::Setup(RenderGraphBuilder& builder)
    {
    }

    Renderer::RenderPassConfig UIPassRenderer::GetRenderPassConfig() const
    {
        return {};
    }

    void UIPassRenderer::Execute(const RenderContext& context)
    {
        // if (!context.RenderState)
        // {
        //     VA_ENGINE_ERROR("[UIPassRenderer] No render state provided by the RenderGraph.");
        //     return;
        // }

        // Create a simple UI quad in normalized coordinates (-1 to +1)
        // auto uiMesh = RenderCommand::s_UIMesh;
        // if (!uiMesh)
        // {
        //     VA_ENGINE_ERROR("[UIPassRenderer] Failed to create UI mesh.");
        //     return;
        // }

        // Use default material for now
        // auto uiMaterial = g_MaterialSystem->GetCachedMaterial(
        //     "DefaultUI",
        //     RenderPassType::UI,
        //     context.RenderState->GetUUID());
        // if (!uiMaterial)
        // {
        //     VA_ENGINE_ERROR("[UIPassRenderer] Failed to get default material.");
        //     return;
        // }
        //
        // // Create geometry render data
        // const auto uiGeometry = Resources::GeometryRenderData(
        //     Math::Mat4::Translate(0.15f * 0.5f, 0.15f * 0.5f, 0.f),
        //     uiMaterial,
        //     uiMesh);
        //
        // // Render the UI quad
        // uiMaterial->Bind(context.Rhi, context.RenderState);
        // context.Rhi.DrawMesh(uiGeometry, context.RenderState);
    }
}
