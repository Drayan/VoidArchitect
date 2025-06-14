//
// Created by Michael Desmedt on 14/05/2025.
//
#include "RenderCommand.hpp"

#include "Camera.hpp"
#include "Core/Logger.hpp"
#include "Core/Window.hpp"
#include "Platform/RHI/IRenderingHardware.hpp"
#include "Platform/RHI/Vulkan/VulkanRhi.hpp"
#include "RenderGraph.hpp"
#include "Resources/Material.hpp"
#include "Systems/MaterialSystem.hpp"
#include "Systems/MeshSystem.hpp"
#include "Systems/RenderStateSystem.hpp"
#include "Systems/ShaderSystem.hpp"
#include "Systems/TextureSystem.hpp"

namespace VoidArchitect::Renderer
{
    Math::Mat4 _RenderCommand::s_UIProjectionMatrix = Math::Mat4::Identity();

    Platform::RHI_API_TYPE _RenderCommand::m_ApiType = Platform::RHI_API_TYPE::Vulkan;
    Platform::IRenderingHardware* _RenderCommand::m_RenderingHardware = nullptr;
    uint32_t _RenderCommand::m_Width = 0;
    uint32_t _RenderCommand::m_Height = 0;
    VAArray<Camera> _RenderCommand::m_Cameras;

    void _RenderCommand::Initialize(
        const Platform::RHI_API_TYPE apiType,
        std::unique_ptr<Window>& window)
    {
        m_ApiType = apiType;

        m_Width = window->GetWidth();
        m_Height = window->GetHeight();

        // Retrieve Pipeline's shared resources setup.
        // TODO This should be managed by the pipeline system.
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

        switch (apiType)
        {
            case Platform::RHI_API_TYPE::Vulkan:
                m_RenderingHardware = new Platform::VulkanRHI(window);
                break;
            default:
                break;
        }

        // Initialize subsystems
        g_ShaderSystem = std::make_unique<ShaderSystem>();
        g_TextureSystem = std::make_unique<TextureSystem>();
        g_RenderPassSystem = std::make_unique<RenderPassSystem>();
        g_RenderStateSystem = std::make_unique<RenderStateSystem>();
        g_MaterialSystem = std::make_unique<MaterialSystem>();
        g_MeshSystem = std::make_unique<MeshSystem>();

        // g_RenderGraph = std::make_unique<RenderGraph>(*m_RenderingHardware);

        // g_RenderGraph->SetupForwardRenderer(m_Width, m_Height);

        // if (!g_RenderGraph->Compile())
        // {
        //     VA_ENGINE_CRITICAL("[RenderCommand] Failed to compile render graph.");
        //     return;
        // }
        //
        // if (!g_RenderGraph)
        // {
        //     VA_ENGINE_CRITICAL("[RenderCommand] Failed to setup render graph.");
        //     return;
        // }

        const float aspectRatio = static_cast<float>(m_Width) / static_cast<float>(m_Height);
        s_UIProjectionMatrix = Math::Mat4::Orthographic(
            0.f,
            1.0f,
            0.f,
            1.0f / aspectRatio,
            -1.0f,
            1.0f);

        CreatePerspectiveCamera(45.0f, 0.1f, 100.0f);

        // SwapTestTexture();
    }

    void _RenderCommand::Shutdown()
    {
        // Shutdown subsystems
        g_MeshSystem = nullptr;
        g_MaterialSystem = nullptr;
        g_RenderStateSystem = nullptr;
        g_RenderPassSystem = nullptr;
        g_TextureSystem = nullptr;
        g_ShaderSystem = nullptr;

        delete m_RenderingHardware;
    }

    void _RenderCommand::Resize(const uint32_t width, const uint32_t height)
    {
        m_Width = width;
        m_Height = height;

        // Update all camera's aspect ratio
        float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
        for (auto& camera : m_Cameras) camera.SetAspectRatio(aspectRatio);

        s_UIProjectionMatrix = Math::Mat4::Orthographic(
            0.f,
            1.0f,
            0.f,
            1.0f / aspectRatio,
            -1.0f,
            1.0f);

        // g_RenderGraph->OnResize(width, height);
        // g_RenderGraph->Compile();
    }

    bool _RenderCommand::BeginFrame(const float deltaTime)
    {
        // Render with the default camera.
        return BeginFrame(m_Cameras[0], deltaTime);
    }

    bool _RenderCommand::BeginFrame(Camera& camera, const float deltaTime)
    {
        if (!m_RenderingHardware->BeginFrame(deltaTime)) return false;

        // if (g_RenderGraph)
        // {
        //     camera.RecalculateView();
        //
        //     FrameData frameData;
        //     frameData.deltaTime = deltaTime;
        //     frameData.Projection = camera.GetProjection();
        //     frameData.View = camera.GetView();
        //
        //     const Resources::GlobalUniformObject gUBO{
        //         .View = frameData.View,
        //         .Projection = frameData.Projection,
        //         .UIProjection = s_UIProjectionMatrix,
        //         .LightDirection = Math::Vec4::Zero() - Math::Vec4(0.f, 1.f, 1.f, 0.f),
        //         .LightColor = Math::Vec4::One(),
        //         .ViewPosition = Math::Vec4(camera.GetPosition(), 1.0f)
        //     };
        //
        //     // Update global state, might be moved elsewhere
        //     m_RenderingHardware->UpdateGlobalState(gUBO);
        //
        //     g_RenderGraph->Execute(frameData);
        // }
        // else
        // {
        //     VA_ENGINE_CRITICAL("[RenderCommand] Render graph is not initialized.");
        //     return false;
        // }

        return true;
    }

    bool _RenderCommand::EndFrame(const float deltaTime)
    {
        return m_RenderingHardware->EndFrame(deltaTime);
    }

    Camera& _RenderCommand::CreatePerspectiveCamera(float fov, float near, float far)
    {
        auto aspect = m_Width / static_cast<float>(m_Height);
        if (m_Width == 0 || m_Height == 0) aspect = 1.0f;
        return m_Cameras.emplace_back(fov, aspect, near, far);
    }

    Camera& _RenderCommand::CreateOrthographicCamera(
        float left,
        float right,
        float bottom,
        float top,
        float near,
        float far)
    {
        return m_Cameras.emplace_back(top, bottom, left, right, near, far);
    }

    void _RenderCommand::SwapTestTexture()
    {
        constexpr std::string textures[] = {
            "wall1_color",
            "wall1_n",
            "wall1_shga",
            "wall2_color",
            "wall2_n",
            "wall2_shga",
            "wall3_color",
            "wall3_n",
            "wall3_shga",
            "wall4_color",
            "wall4_n",
            "wall4_shga"
        };
        static size_t index = std::size(textures) - 1;
        index = (index + 1) % std::size(textures);

        //s_TestMaterial->SetTexture(/*Resources::TextureSlot::Diffuse*/ 0, s_TestTexture);
    }

    void _RenderCommand::SwapColor()
    {
        const Math::Vec4 colors[] = {
            Math::Vec4(1.0f, 0.0f, 0.0f, 1.0f),
            Math::Vec4(0.0f, 1.0f, 0.0f, 1.0f),
            Math::Vec4(0.0f, 0.0f, 1.0f, 1.0f),
            Math::Vec4(1.0f, 1.0f, 0.0f, 1.0f),
            Math::Vec4(1.0f, 0.0f, 1.0f, 1.0f),
            Math::Vec4(0.0f, 1.0f, 1.0f, 1.0f),
            Math::Vec4(1.0f, 1.0f, 1.0f, 1.0f),
        };
        static size_t index = std::size(colors) - 1;
        index = (index + 1) % std::size(colors);

        //s_TestMaterial->SetDiffuseColor(colors[index]);
    }
} // namespace VoidArchitect::Renderer
