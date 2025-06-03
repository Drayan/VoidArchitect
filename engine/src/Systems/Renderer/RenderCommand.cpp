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
    Resources::MeshPtr RenderCommand::s_TestMesh;

    std::unique_ptr<RenderGraph> RenderCommand::s_RenderGraph;

    Platform::RHI_API_TYPE RenderCommand::m_ApiType = Platform::RHI_API_TYPE::Vulkan;
    Platform::IRenderingHardware* RenderCommand::m_RenderingHardware = nullptr;
    uint32_t RenderCommand::m_Width = 0;
    uint32_t RenderCommand::m_Height = 0;
    std::vector<Camera> RenderCommand::m_Cameras;

    void RenderCommand::Initialize(
        const Platform::RHI_API_TYPE apiType, std::unique_ptr<Window>& window)
    {
        m_ApiType = apiType;

        m_Width = window->GetWidth();
        m_Height = window->GetHeight();

        // Retrieve Pipeline's shared resources setup.
        // TODO This should be managed by the pipeline system.
        const RenderStateInputLayout sharedInputLayout{std::vector{
            SpaceLayout{
                0,
                std::vector{ResourceBinding{
                    ResourceBindingType::ConstantBuffer, 0, Resources::ShaderStage::Vertex}},
            },
            SpaceLayout{
                1,
                std::vector{
                    ResourceBinding{
                        ResourceBindingType::ConstantBuffer, 0, Resources::ShaderStage::Pixel},
                    ResourceBinding{
                        ResourceBindingType::Texture2D, 1, Resources::ShaderStage::Pixel}}}}};

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
        g_RenderStateSystem = std::make_unique<RenderStateSystem>();
        g_MaterialSystem = std::make_unique<MaterialSystem>();
        g_MeshSystem = std::make_unique<MeshSystem>();

        s_RenderGraph = std::make_unique<RenderGraph>(*m_RenderingHardware);
        s_RenderGraph->SetupForwardRenderer(m_Width, m_Height);

        if (!s_RenderGraph->Compile())
        {
            VA_ENGINE_CRITICAL("[RenderCommand] Failed to compile render graph.");
            return;
        }

        if (!s_RenderGraph)
        {
            VA_ENGINE_CRITICAL("[RenderCommand] Failed to setup render graph.");
            return;
        }

        // TEMP Try to load a test material.
        s_TestMaterial = g_MaterialSystem->LoadMaterial("TestMaterial");

        // TEMP Create a test mesh.
        s_TestMesh = g_MeshSystem->CreateCube("TestMesh");

        CreatePerspectiveCamera(45.0f, 0.1f, 100.0f);

        SwapTestTexture();
    }

    void RenderCommand::Shutdown()
    {
        // Wait that any pending operation is completed before beginning the shutdown procedure.
        m_RenderingHardware->WaitIdle(0);

        s_RenderGraph = nullptr;

        s_TestMesh = nullptr;
        s_TestMaterial = nullptr;
        s_TestTexture = nullptr;

        // Shutdown subsystems
        g_MeshSystem = nullptr;
        g_MaterialSystem = nullptr;
        g_RenderStateSystem = nullptr;
        g_TextureSystem = nullptr;
        g_ShaderSystem = nullptr;

        delete m_RenderingHardware;
    }

    void RenderCommand::Resize(const uint32_t width, const uint32_t height)
    {
        m_Width = width;
        m_Height = height;

        // Update all camera's aspect ratio
        for (auto& camera : m_Cameras)
            camera.SetAspectRatio(width / static_cast<float>(height));

        s_RenderGraph->OnResize(width, height);
        s_RenderGraph->Compile();
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

        if (s_RenderGraph)
        {
            camera.RecalculateView();

            FrameData frameData;
            frameData.deltaTime = deltaTime;
            frameData.Projection = camera.GetProjection();
            frameData.View = camera.GetView();

            s_RenderGraph->Execute(frameData);
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
        float left, float right, float bottom, float top, float near, float far)
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
            "wall4_shga"};
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
