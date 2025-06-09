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
            if (!m_RHI->BeginFrame(frameTime))
            {
                VA_ENGINE_ERROR("[RenderSystem] Failed to begin the frame.");
                return;
            }

            // Create a new RenderGraph for this frame.
            m_RenderGraph = {};

            // Add passes to the graph.
            m_RenderGraph.AddPass("ForwardOpaque", &m_ForwardOpaquePassRenderer);
            m_RenderGraph.AddPass("UI", &m_UIPassRenderer);

            // Set up the graph.
            m_RenderGraph.Setup();

            // Compile the graph.
            auto executionOrder = m_RenderGraph.Compile();

            // Execute rendering.
            for (const auto& pass : executionOrder)
            {
                RenderContext context{*m_RHI};
                pass->Execute(context);
            }

            m_RHI->EndFrame(frameTime);
        }
    } // Renderer
} // VoidArchitect
