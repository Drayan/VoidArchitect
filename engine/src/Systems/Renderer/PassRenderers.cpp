//
// Created by Michael Desmedt on 04/06/2025.
//
#include "PassRenderers.hpp"

#include "Core/Logger.hpp"
#include "Platform/RHI/IRenderingHardware.hpp"
#include "RenderCommand.hpp"
#include "RenderGraphBuilder.hpp"
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
        // TEMP We create the mesh here only for testing purposes.
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
                Math::Vec4(0.1f, 0.1f, 0.1f, 1.0f)
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
        angle += (0.2f * context.frameData.deltaTime);
        auto handle = g_MeshSystem->GetHandleFor("TestCube");
        const auto geometry = Resources::GeometryRenderData(
            Math::Mat4::Rotate(angle, Math::Vec3::Up()),
            testMat,
            handle);

        //TODO: Get materials from submeshes
        const RenderStateCacheKey key = {
            g_MaterialSystem->GetClass(testMat),
            RenderPassType::ForwardOpaque,
            VertexFormat::PositionNormalUVTangent,
            context.currentPassSignature
        };
        const auto stateHandle = g_RenderStateSystem->GetHandleFor(key, context.currentPassHandle);
        context.rhi.BindRenderState(stateHandle);
        context.rhi.BindMaterial(geometry.Material, stateHandle);

        //TODO: Implement Transform and get worldTransform from there = modelMatrix
        context.rhi.PushConstants(
            Resources::ShaderStage::Vertex,
            sizeof(Math::Mat4),
            &geometry.Model);
        context.rhi.BindMesh(geometry.Mesh);
        //TODO: Draw each submeshes sorted by material handle
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

        // Render the UI quad
        const RenderStateCacheKey key = {
            g_MaterialSystem->GetClass(uiMaterial),
            RenderPassType::UI,
            VertexFormat::PositionNormalUVTangent,
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
} // namespace VoidArchitect::Renderer
