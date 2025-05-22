//
// Created by Michael Desmedt on 14/05/2025.
//
#include "RenderCommand.hpp"

#include "Camera.hpp"
#include "Core/Logger.hpp"
#include "Core/Window.hpp"
#include "Platform/RHI/IRenderingHardware.hpp"
#include "Platform/RHI/Material.hpp"
#include "Platform/RHI/Vulkan/VulkanRhi.hpp"
#include "Resources/Texture.hpp"

namespace VoidArchitect::Renderer
{
    std::shared_ptr<Resources::Texture2D> RenderCommand::m_DefaultTexture;

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

        // TEMP Create a default camera until we have a real scene manager
        CreatePerspectiveCamera(45.0f, 0.1f, 100.0f);

        // TEMP Create a default texture that should be used if no/invalid texture are provided.
        VA_ENGINE_TRACE("[RenderCommand] Creating default texture.");
        constexpr uint32_t texSize = 256;
        constexpr uint32_t texChannels = 4;

        std::vector<uint8_t> texData(texSize * texSize * texChannels);
        for (uint32_t y = 0; y < texSize; ++y)
        {
            for (uint32_t x = 0; x < texSize; ++x)
            {
                constexpr uint8_t magenta[4] = {255, 0, 255, 255};
                constexpr uint8_t white[4] = {255, 255, 255, 255};
                constexpr uint32_t squareSize = 32;

                const auto squareX = x / squareSize;
                const auto squareY = y / squareSize;

                const bool isWhite = (squareX + squareY) % 2 == 0;

                const auto pixelIndex = (y * texSize + x) * texChannels;

                const uint8_t* color = isWhite ? white : magenta;
                texData[pixelIndex + 0] = color[0];
                texData[pixelIndex + 1] = color[1];
                texData[pixelIndex + 2] = color[2];
                texData[pixelIndex + 3] = color[3];
            }
        }

        m_DefaultTexture = CreateTexture2D(texSize, texSize, texChannels, true, texData);
        VA_ENGINE_TRACE("[RenderCommand] Default texture created.");
    }

    void RenderCommand::Shutdown()
    {
        m_DefaultTexture.reset();
        VA_ENGINE_TRACE("[RenderCommand] Default texture destroyed.");

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
        geometry.Textures[0] = std::dynamic_pointer_cast<Resources::ITexture>(m_DefaultTexture);

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

    std::shared_ptr<Resources::Texture2D> RenderCommand::CreateTexture2D(const std::string& name)
    {
        std::vector<uint8_t> data;
        switch (m_ApiType)
        {
            case Platform::RHI_API_TYPE::Vulkan:
                //return m_RenderingHardware->CreateTexture2D(TODO, TODO, TODO, TODO, data);
                return nullptr;
            default:
                break;
        }

        VA_ENGINE_WARN("[RenderCommand] Failed to create a texture {}.", name);
        return nullptr;
    }

    std::shared_ptr<Resources::Texture2D> RenderCommand::CreateTexture2D(
        const uint32_t width,
        const uint32_t height,
        const uint8_t channels,
        const bool hasTransparency,
        const std::vector<uint8_t>& data)
    {
        switch (m_ApiType)
        {
            case Platform::RHI_API_TYPE::Vulkan:
                return m_RenderingHardware->CreateTexture2D(
                    width,
                    height,
                    channels,
                    hasTransparency,
                    data);
            default:
                break;
        }

        VA_ENGINE_WARN("[RenderCommand] Failed to create a texture.");
        return nullptr;
    }
} // VoidArchitect
