//
// Created by Michael Desmedt on 07/06/2025.
//
#include "RenderSystem.hpp"

#include "PassRenderers.hpp"
#include "RenderGraph.hpp"
#include "Core/Logger.hpp"
#include "Core/Window.hpp"
#include "Platform/RHI/IRenderingHardware.hpp"
#include "Platform/RHI/Vulkan/VulkanRhi.hpp"
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
        {
            m_ApiType = apiType;

            m_Width = window->GetWidth();
            m_Height = window->GetHeight();

            // Retrieve Pipeline's shared resources setup.
            // TODO: Actually this should be revised.
            // const RenderStateInputLayout sharedInputLayout{
            //     VAArray{
            //         // Global Space
            //         SpaceLayout{
            //             0,
            //             VAArray{
            //                 // Global UBO
            //                 ResourceBinding{
            //                     ResourceBindingType::ConstantBuffer,
            //                     0,
            //                     Resources::ShaderStage::All,
            //                     std::vector{
            //                         // Projection
            //                         BufferBinding{0, AttributeType::Mat4, AttributeFormat::Float32},
            //                         // View
            //                         BufferBinding{1, AttributeType::Mat4, AttributeFormat::Float32},
            //                         // UI Projection
            //                         BufferBinding{2, AttributeType::Mat4, AttributeFormat::Float32},
            //                         // Light Direction
            //                         BufferBinding{3, AttributeType::Vec4, AttributeFormat::Float32},
            //                         // Light Color
            //                         BufferBinding{4, AttributeType::Vec4, AttributeFormat::Float32}
            //                     },
            //                 }
            //             },
            //         },
            //         // Material Space
            //         SpaceLayout{
            //             1,
            //             VAArray{
            //                 // Material UBO
            //                 ResourceBinding{
            //                     ResourceBindingType::ConstantBuffer,
            //                     0,
            //                     Resources::ShaderStage::Pixel,
            //                     std::vector{
            //                         // Diffuse Color
            //                         BufferBinding{0, AttributeType::Vec4, AttributeFormat::Float32}
            //                     }
            //                 },
            //                 // Diffuse Map
            //                 ResourceBinding{
            //                     ResourceBindingType::Texture2D,
            //                     1,
            //                     Resources::ShaderStage::Pixel
            //                 },
            //                 // Specular Map
            //                 ResourceBinding{
            //                     ResourceBindingType::Texture2D,
            //                     2,
            //                     Resources::ShaderStage::Pixel
            //                 }
            //             }
            //         }
            //     }
            // };

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

            // --- Import persistent resources ---
            m_RenderGraph.ImportRenderTarget(
                WELL_KNOWN_RT_VIEWPORT_COLOR,
                m_RHI->GetCurrentColorRenderTargetHandle());
            m_RenderGraph.ImportRenderTarget(
                WELL_KNOWN_RT_VIEWPORT_DEPTH,
                m_RHI->GetDepthRenderTargetHandle());

            // --- Add passes to the graph. ---
            m_RenderGraph.AddPass("ForwardOpaque", &m_ForwardOpaquePassRenderer);
            // m_RenderGraph.AddPass("UI", &m_UIPassRenderer);

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
            if (!m_RHI->BeginFrame(frameTime))
            {
                VA_ENGINE_ERROR("[RenderSystem] Failed to begin the frame.");
                return;
            }

            RenderContext context{*m_RHI};
            for (const auto& step : executionPlan)
            {
                // Ask the RenderPassSystem the handle for the config of this pass.
                const RenderPassHandle passHandle = g_RenderPassSystem->GetHandleFor(
                    step.passConfig,
                    step.passPosition);

                // Get the handles of RenderTargets
                m_RHI->BeginRenderPass(passHandle, step.renderTargets);
                step.passRenderer->Execute(context);
                m_RHI->EndRenderPass();
            }

            m_RHI->EndFrame(frameTime);
        }
    } // Renderer
} // VoidArchitect
