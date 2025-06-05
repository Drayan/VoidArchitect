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
    Resources::Texture2DPtr RenderCommand::s_TestTexture;
    Resources::MaterialPtr RenderCommand::s_TestMaterial;
    Resources::MaterialPtr RenderCommand::s_UIMaterial;
    Resources::MeshPtr RenderCommand::s_TestMesh;
    Resources::MeshPtr RenderCommand::s_UIMesh;

    Math::Mat4 RenderCommand::s_UIProjectionMatrix = Math::Mat4::Identity();

    Platform::RHI_API_TYPE RenderCommand::m_ApiType = Platform::RHI_API_TYPE::Vulkan;
    Platform::IRenderingHardware* RenderCommand::m_RenderingHardware = nullptr;
    uint32_t RenderCommand::m_Width = 0;
    uint32_t RenderCommand::m_Height = 0;
    VAArray<Camera> RenderCommand::m_Cameras;

    void RenderCommand::Initialize(
        const Platform::RHI_API_TYPE apiType,
        std::unique_ptr<Window>& window)
    {
        m_ApiType = apiType;

        m_Width = window->GetWidth();
        m_Height = window->GetHeight();

        // Retrieve Pipeline's shared resources setup.
        // TODO This should be managed by the pipeline system.
        const RenderStateInputLayout sharedInputLayout{
            VAArray{
                SpaceLayout{
                    0,
                    VAArray{
                        ResourceBinding{
                            ResourceBindingType::ConstantBuffer,
                            0,
                            Resources::ShaderStage::All
                        }
                    },
                },
                SpaceLayout{
                    1,
                    VAArray{
                        ResourceBinding{
                            ResourceBindingType::ConstantBuffer,
                            0,
                            Resources::ShaderStage::Pixel
                        },
                        ResourceBinding{
                            ResourceBindingType::Texture2D,
                            1,
                            Resources::ShaderStage::Pixel
                        }
                    }
                }
            }
        };

        switch (apiType)
        {
            case Platform::RHI_API_TYPE::Vulkan:
                m_RenderingHardware = new Platform::VulkanRHI(window, sharedInputLayout);
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

        g_RenderGraph = std::make_unique<RenderGraph>(*m_RenderingHardware);
        g_RenderGraph->SetupForwardRenderer(m_Width, m_Height);

        if (!g_RenderGraph->Compile())
        {
            VA_ENGINE_CRITICAL("[RenderCommand] Failed to compile render graph.");
            return;
        }

        if (!g_RenderGraph)
        {
            VA_ENGINE_CRITICAL("[RenderCommand] Failed to setup render graph.");
            return;
        }

        // TEMP Try to load a test material.
        s_TestMaterial = g_MaterialSystem->LoadMaterial("TestMaterial");
        s_UIMaterial = g_MaterialSystem->LoadMaterial("DefaultUI");

        const float aspectRatio = static_cast<float>(m_Width) / static_cast<float>(m_Height);
        s_UIProjectionMatrix = Math::Mat4::Orthographic(
            0.f,
            1.0f,
            1.0f / aspectRatio,
            0.f,
            -1.0f,
            1.0f);

        // TEMP Create a test mesh.
        s_TestMesh = g_MeshSystem->CreateCube("TestMesh");
        s_UIMesh = g_MeshSystem->CreatePlane("UIMesh", 0.15f, 0.15f, Math::Vec3::Back());

        CreatePerspectiveCamera(45.0f, 0.1f, 100.0f);

        SwapTestTexture();
    }

    void RenderCommand::Shutdown()
    {
        // Wait that any pending operation is completed before beginning the shutdown procedure.
        m_RenderingHardware->WaitIdle(0);

        g_RenderGraph = nullptr;

        s_UIMesh = nullptr;
        s_TestMesh = nullptr;
        s_UIMaterial = nullptr;
        s_TestMaterial = nullptr;
        s_TestTexture = nullptr;

        // Shutdown subsystems
        g_MeshSystem = nullptr;
        g_MaterialSystem = nullptr;
        g_RenderStateSystem = nullptr;
        g_RenderPassSystem = nullptr;
        g_TextureSystem = nullptr;
        g_ShaderSystem = nullptr;

        delete m_RenderingHardware;
    }

    void RenderCommand::Resize(const uint32_t width, const uint32_t height)
    {
        m_Width = width;
        m_Height = height;

        // Update all camera's aspect ratio
        float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
        for (auto& camera : m_Cameras)
            camera.SetAspectRatio(aspectRatio);

        s_UIProjectionMatrix = Math::Mat4::Orthographic(
            0.f,
            1.0f,
            1.0f / aspectRatio,
            0.f,
            -1.0f,
            1.0f);

        g_RenderGraph->OnResize(width, height);
        g_RenderGraph->Compile();
    }

    bool RenderCommand::BeginFrame(const float deltaTime)
    {
        // Render with the default camera.
        return BeginFrame(m_Cameras[0], deltaTime);
    }

    bool RenderCommand::BeginFrame(Camera& camera, const float deltaTime)
    {
        if (!m_RenderingHardware->BeginFrame(deltaTime))
            return false;

        if (g_RenderGraph)
        {
            camera.RecalculateView();

            FrameData frameData;
            frameData.deltaTime = deltaTime;
            frameData.Projection = camera.GetProjection();
            frameData.View = camera.GetView();

            const Resources::GlobalUniformObject gUBO{
                .View = frameData.View,
                .Projection = frameData.Projection,
                .UIProjection = s_UIProjectionMatrix,
                .LightDirection = Math::Vec4::Zero() - Math::Vec4(0.f, 1.f, 1.f, 0.f),
                .LightColor = Math::Vec4::One()
            };

            // Update global state, might be moved elsewhere
            m_RenderingHardware->UpdateGlobalState(gUBO);

            g_RenderGraph->Execute(frameData);
        }
        else
        {
            VA_ENGINE_CRITICAL("[RenderCommand] Render graph is not initialized.");
            return false;
        }

        return true;
    }

    bool RenderCommand::EndFrame(const float deltaTime)
    {
        return m_RenderingHardware->EndFrame(deltaTime);
    }

    Camera& RenderCommand::CreatePerspectiveCamera(float fov, float near, float far)
    {
        auto aspect = m_Width / static_cast<float>(m_Height);
        if (m_Width == 0 || m_Height == 0)
            aspect = 1.0f;
        return m_Cameras.emplace_back(fov, aspect, near, far);
    }

    Camera& RenderCommand::CreateOrthographicCamera(
        float left,
        float right,
        float bottom,
        float top,
        float near,
        float far)
    {
        return m_Cameras.emplace_back(top, bottom, left, right, near, far);
    }

    void RenderCommand::SwapTestTexture()
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

        s_TestTexture =
            g_TextureSystem->LoadTexture2D(textures[index], Resources::TextureUse::Diffuse);
        s_TestMaterial->SetTexture(/*Resources::TextureSlot::Diffuse*/ 0, s_TestTexture);
    }

    void RenderCommand::SwapColor()
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

        s_TestMaterial->SetDiffuseColor(colors[index]);
    }
} // namespace VoidArchitect::Renderer
