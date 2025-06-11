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
#include "Systems/MeshSystem.hpp"
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
        g_MeshSystem->CreateCube("TestCube");
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
        const auto testMat = g_MaterialSystem->GetHandleFor("TestMaterial");
        if (testMat == InvalidMaterialHandle)
        {
            VA_ENGINE_ERROR("[ForwardOpaquePassRenderer] Failed to get test material.");
            return;
        }

        // Render test geometry
        static float angle = 0.f;
        angle += (0.5f * context.frameData.deltaTime);
        auto handle = g_MeshSystem->GetHandleFor("TestCube");
        const auto geometry = Resources::GeometryRenderData(
            Math::Mat4::Rotate(angle, Math::Vec3::Up()),
            testMat,
            handle);

        const RenderStateCacheKey key = {
            g_MaterialSystem->GetClass(testMat),
            RenderPassType::ForwardOpaque,
            VertexFormat::PositionNormalUV,
            context.currentPassSignature
        };
        const auto stateHandle = g_RenderStateSystem->GetHandleFor(key, context.currentPassHandle);
        context.rhi.BindRenderState(stateHandle);
        context.rhi.BindMaterial(geometry.Material, stateHandle);

        context.rhi.PushConstants(
            Resources::ShaderStage::Vertex,
            sizeof(Math::Mat4),
            &geometry.Model);
        context.rhi.BindMesh(geometry.Mesh);
        context.rhi.DrawIndexed(g_MeshSystem->GetIndexCountFor(handle));
    }

    //==============================================================================================
    // UIPassRenderer Implementation
    //==============================================================================================

    void UIPassRenderer::Setup(RenderGraphBuilder& builder)
    {
        g_MeshSystem->CreateQuad("UIQuad", 0.15f, 0.15f);
        builder.ReadsFromColorBuffer().WritesToColorBuffer();
    }

    Renderer::RenderPassConfig UIPassRenderer::GetRenderPassConfig() const
    {
        RenderPassConfig config;
        config.attachments = {
            {"color", TextureFormat::SWAPCHAIN_FORMAT, LoadOp::Load, StoreOp::Store,}
        };
        config.type = RenderPassType::UI;
        config.name = m_Name;
        return config;
    }

    void UIPassRenderer::Execute(const RenderContext& context)
    {
        // Create a simple UI quad in normalized coordinates (-1 to +1)
        // auto uiMesh = RenderCommand::s_UIMesh;
        // if (!uiMesh)
        // {
        //     VA_ENGINE_ERROR("[UIPassRenderer] Failed to create UI mesh.");
        //     return;
        // }
        const auto uiMaterial = g_MaterialSystem->GetHandleFor("DefaultUI");
        if (uiMaterial == InvalidMaterialHandle)
        {
            VA_ENGINE_ERROR("[UIPassRenderer] Failed to get default material.");
            return;
        }

        // Create geometry render data
        const auto uiGeometry = Resources::GeometryRenderData(
            Math::Mat4::Translate(0.15f * 0.5f, 0.15f * 0.5f, 0.f),
            uiMaterial,
            g_MeshSystem->GetHandleFor("UIQuad"));

        // // Render the UI quad
        const RenderStateCacheKey key = {
            g_MaterialSystem->GetClass(uiMaterial),
            RenderPassType::UI,
            VertexFormat::PositionNormalUV,
            context.currentPassSignature
        };
        const auto stateHandle = g_RenderStateSystem->GetHandleFor(key, context.currentPassHandle);
        context.rhi.BindRenderState(stateHandle);
        context.rhi.BindMaterial(uiGeometry.Material, stateHandle);

        context.rhi.PushConstants(
            Resources::ShaderStage::Vertex,
            sizeof(Math::Mat4),
            &uiGeometry.Model);
        context.rhi.BindMesh(uiGeometry.Mesh);
        context.rhi.DrawIndexed(g_MeshSystem->GetIndexCountFor(uiGeometry.Mesh));
    }
}
