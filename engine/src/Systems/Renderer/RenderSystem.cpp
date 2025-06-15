//
// Created by Michael Desmedt on 07/06/2025.
//
#include "RenderSystem.hpp"

#include "Core/Logger.hpp"
#include "Core/Window.hpp"
#include "PassRenderers.hpp"
#include "Platform/RHI/IRenderingHardware.hpp"
#include "Platform/RHI/Vulkan/VulkanRhi.hpp"
#include "RenderGraph.hpp"
#include "Systems/MaterialSystem.hpp"
#include "Systems/MeshSystem.hpp"
#include "Systems/RenderStateSystem.hpp"
#include "Systems/ShaderSystem.hpp"
#include "Systems/TextureSystem.hpp"

namespace VoidArchitect
{
    namespace Renderer
    {
        RenderSystem::RenderSystem(Platform::RHI_API_TYPE apiType, std::unique_ptr<Window>& window)
            : m_MainCamera(45.0f, 1.f, 0.1f, 1000.f)
        {
            m_ApiType = apiType;

            m_Width = window->GetWidth();
            m_Height = window->GetHeight();

            m_MainCamera.SetPosition(Math::Vec3(0.f, 0.f, 3.f));
            m_MainCamera.SetAspectRatio(static_cast<float>(m_Width) / static_cast<float>(m_Height));

            // Initialize the RHI
            switch (apiType)
            {
                case Platform::RHI_API_TYPE::Vulkan:
                    m_RHI = std::make_unique<Platform::VulkanRHI>(window);
                    break;
                default:
                    break;
            }

            if (!m_RHI)
            {
                VA_ENGINE_CRITICAL("[RenderSystem] Failed to initialize the RHI.");
                throw std::runtime_error("Failed to initialize the RHI.");
            }
        }

        void RenderSystem::InitializeSubsystems()
        {
            // Initialize subsystems
            g_ShaderSystem = std::make_unique<ShaderSystem>();
            g_TextureSystem = std::make_unique<TextureSystem>();
            g_RenderPassSystem = std::make_unique<RenderPassSystem>();
            g_RenderStateSystem = std::make_unique<RenderStateSystem>();
            g_MaterialSystem = std::make_unique<MaterialSystem>();
            g_MeshSystem = std::make_unique<MeshSystem>();
        }

        RenderSystem::~RenderSystem()
        {
            m_RHI->WaitIdle();
            // Shutdown subsystems
            g_MeshSystem = nullptr;
            g_MaterialSystem = nullptr;
            g_RenderStateSystem = nullptr;
            g_RenderPassSystem = nullptr;
            g_TextureSystem = nullptr;
            g_ShaderSystem = nullptr;

            m_RHI = nullptr;
        }

        void RenderSystem::RenderFrame(const float frameTime)
        {
            m_RenderGraph = {};
            if (!m_RHI->BeginFrame(frameTime))
            {
                VA_ENGINE_DEBUG("[RenderSystem] Failed to begin the frame.");
                return;
            }

            // --- Import persistent resources ---
            m_RenderGraph.ImportRenderTarget(
                WELL_KNOWN_RT_VIEWPORT_COLOR,
                m_RHI->GetCurrentColorRenderTargetHandle());
            m_RenderGraph.ImportRenderTarget(
                WELL_KNOWN_RT_VIEWPORT_DEPTH,
                m_RHI->GetDepthRenderTargetHandle());

            // --- Add passes to the graph. ---
            m_RenderGraph.AddPass("ForwardOpaque", &m_ForwardOpaquePassRenderer);
            m_RenderGraph.AddPass("UI", &m_UIPassRenderer);

            // --- Set up the graph. ---
            m_RenderGraph.Setup();

            // --- Step 3: Compilation ---
            // Compile the graph.
            auto executionPlan = m_RenderGraph.Compile();
            if (executionPlan.empty())
            {
                VA_ENGINE_ERROR(
                    "[RenderSystem] Render graph compilation failed or resulted in an empty plan.");
                return;
            }

            // --- Step 4: Execution ---
            // Execute rendering.

            m_MainCamera.RecalculateView();
            const auto aspectRatio = static_cast<float>(m_Width) / static_cast<float>(m_Height);
            const auto UIProjectionMatrix = Math::Mat4::Orthographic(
                0.f,
                1.0f,
                0.f,
                1.0f / aspectRatio,
                -1.0f,
                1.0f);

            Resources::GlobalUniformObject ubo{};
            ubo.View = m_MainCamera.GetView();
            ubo.Projection = m_MainCamera.GetProjection();
            ubo.LightColor = Math::Vec4::One();
            ubo.LightDirection = Math::Vec4::Zero() - Math::Vec4(0.f, 1.f, 1.f, 0.f);
            ubo.ViewPosition = Math::Vec4(m_MainCamera.GetPosition(), 1.0f);
            ubo.UIProjection = UIProjectionMatrix;
            ubo.DebugMode = static_cast<uint32_t>(m_DebugMode); // No debug mode by default

            m_RHI->UpdateGlobalState(ubo);
            for (const auto& step : executionPlan)
            {
                // Ask the RenderPassSystem the handle for the config of this pass.
                const RenderPassHandle passHandle = g_RenderPassSystem->GetHandleFor(
                    step.passConfig,
                    step.passPosition);

                const auto& passSignature = g_RenderPassSystem->GetSignatureFor(passHandle);
                RenderContext context{*m_RHI.get(), {frameTime}, passHandle, passSignature};
                // Get the handles of RenderTargets
                m_RHI->BeginRenderPass(passHandle, step.renderTargets);
                step.passRenderer->Execute(context);
                m_RHI->EndRenderPass();
            }

            m_RHI->EndFrame(frameTime);
        }

        void RenderSystem::Resize(const uint32_t width, const uint32_t height)
        {
            m_Width = width;
            m_Height = height;
            m_MainCamera.SetAspectRatio(static_cast<float>(width) / static_cast<float>(height));

            m_RHI->Resize(width, height);
        }
    } // namespace Renderer
} // namespace VoidArchitect
