//
// Created by Michael Desmedt on 14/05/2025.
//
#include "RenderCommand.hpp"

//TEMP Temporary block
#include <stb_image.h>
//TEMP End of temporary

#include "Camera.hpp"
#include "Core/Logger.hpp"
#include "Core/Window.hpp"
#include "Platform/RHI/IRenderingHardware.hpp"
#include "Platform/RHI/Material.hpp"
#include "Platform/RHI/Vulkan/VulkanRhi.hpp"
#include "Resources/Texture.hpp"
#include "Systems/TextureSystem.hpp"

namespace VoidArchitect::Renderer
{
    Resources::Texture2DPtr RenderCommand::s_TestTexture;

    Platform::RHI_API_TYPE RenderCommand::m_ApiType = Platform::RHI_API_TYPE::Vulkan;
    Platform::IRenderingHardware* RenderCommand::m_RenderingHardware = nullptr;
    uint32_t RenderCommand::m_Width = 0;
    uint32_t RenderCommand::m_Height = 0;
    std::vector<Camera> RenderCommand::m_Cameras;

    void RenderCommand::Initialize(
        const Platform::RHI_API_TYPE apiType,
        std::unique_ptr<Window>& window)
    {
        m_ApiType = apiType;

        m_Width = window->GetWidth();
        m_Height = window->GetHeight();

        switch (apiType)
        {
            case Platform::RHI_API_TYPE::Vulkan:
                m_RenderingHardware = new Platform::VulkanRHI(window);
                break;
            default:
                break;
        }

        // Initialize subsystems
        g_TextureSystem = std::make_unique<TextureSystem>();

        // TEMP Create a default camera until we have a real scene manager
        CreatePerspectiveCamera(45.0f, 0.1f, 100.0f);

        SwapTestTexture();
    }

    void RenderCommand::Shutdown()
    {
        // Wait that any pending operation is completed before beginning the shutdown procedure.
        m_RenderingHardware->WaitIdle(0);

        s_TestTexture = nullptr;

        VA_ENGINE_TRACE("[RenderCommand] Default texture destroyed.");

        // Shutdown subsystems
        g_TextureSystem = nullptr;

        delete m_RenderingHardware;
    }

    void RenderCommand::Resize(const uint32_t width, const uint32_t height)
    {
        m_Width = width;
        m_Height = height;

        // Update all camera's aspect ratio
        for (auto& camera : m_Cameras)
            camera.SetAspectRatio(width / static_cast<float>(height));

        m_RenderingHardware->Resize(width, height);
    }

    bool RenderCommand::BeginFrame(const float deltaTime)
    {
        // Render with the default camera.
        return BeginFrame(m_Cameras[0], deltaTime);
    }

    bool RenderCommand::BeginFrame(Camera& camera, const float deltaTime)
    {
        if (!m_RenderingHardware->BeginFrame(deltaTime)) return false;

        auto geometry = GeometryRenderData(UUID(0), Math::Mat4::Identity());
        geometry.Textures[0] = std::dynamic_pointer_cast<Resources::ITexture>(s_TestTexture);

        camera.RecalculateView();
        m_RenderingHardware->UpdateGlobalState(camera.GetProjection(), camera.GetView());
        m_RenderingHardware->UpdateObjectState(geometry);
        return true;
    }

    bool RenderCommand::EndFrame(const float deltaTime)
    {
        return m_RenderingHardware->EndFrame(deltaTime);
    }

    Camera& RenderCommand::CreatePerspectiveCamera(float fov, float near, float far)
    {
        auto aspect = m_Width / static_cast<float>(m_Height);
        if (m_Width == 0 || m_Height == 0) aspect = 1.0f;
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

        s_TestTexture = g_TextureSystem->LoadTexture2D(textures[index]);
    }
} // VoidArchitect
