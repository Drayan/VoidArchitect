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
        g_MeshSystem->CreateCube("TestCube", "TestMaterial");
        builder.ReadsFrom("TestCube").ReadsFrom("TestMaterial").WritesToColorBuffer().
                WritesToDepthBuffer();
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

        const auto testMesh = g_MeshSystem->GetHandleFor("sponza");
        if (testMesh == Resources::InvalidMeshHandle)
        {
            VA_ENGINE_ERROR("[ForwardOpaquePassRenderer] Failed to get test mesh.");
            return;
        }

        // Render test geometry
        static float angle = 0.f;
        //angle += (0.2f * context.frameData.deltaTime);
        const auto transformMatrix = Math::Mat4::Translate({0.0f, std::sinf(angle), 0.0f}) *
            Math::Mat4::Rotate(angle, Math::Vec3::Up());

        context.rhi.BindMesh(testMesh);

        for (uint32_t i = 0; i < g_MeshSystem->GetSubMeshCountFor(testMesh); i++)
        {
            const auto& submesh = g_MeshSystem->GetSubMesh(testMesh, i);
            const auto materialToUse = (submesh.material != InvalidMaterialHandle)
                ? submesh.material
                : testMat;

            // NOTE: Currently we just use the same renderState for submeshes, we will need
            //      to filter submeshes at some point, e.g. transparent vs opaque submeshes.
            const RenderStateCacheKey key = {
                g_MaterialSystem->GetClass(materialToUse),
                RenderPassType::ForwardOpaque,
                VertexFormat::PositionNormalUVTangent,
                context.currentPassSignature
            };
            const auto stateHandle = g_RenderStateSystem->GetHandleFor(
                key,
                context.currentPassHandle);

            context.rhi.BindRenderState(stateHandle);

            //NOTE: We don't need to push constant everytime as only one transform exist currently
            //TODO: Implement Transform and get worldTransform from there = modelMatrix
            context.rhi.PushConstants(
                Resources::ShaderStage::Vertex,
                sizeof(Math::Mat4),
                &transformMatrix);

            context.rhi.BindMaterial(materialToUse, stateHandle);

            //TODO: Draw each submeshes sorted by material handle
            context.rhi.DrawIndexed(submesh.indexCount, submesh.indexOffset, submesh.vertexOffset);
        }
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
