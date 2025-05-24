//
// Created by Michael Desmedt on 14/05/2025.
//
#pragma once

#include "Camera.hpp"

namespace VoidArchitect
{
    namespace Resources
    {
        class Texture2D;
    }

    class Window;

    namespace Platform
    {
        enum class RHI_API_TYPE;
        class IRenderingHardware;
    }

    namespace Renderer
    {
        class RenderCommand
        {
        public:
            static void Initialize(Platform::RHI_API_TYPE apiType, std::unique_ptr<Window>& window);
            static void Shutdown();

            static void Resize(uint32_t width, uint32_t height);

            static bool BeginFrame(float deltaTime);
            static bool BeginFrame(Camera& camera, float deltaTime);
            static bool EndFrame(float deltaTime);

            static Camera& CreatePerspectiveCamera(float fov, float near, float far);
            static Camera& CreateOrthographicCamera(
                float left,
                float right,
                float bottom,
                float top,
                float near,
                float far);

            static Platform::RHI_API_TYPE GetApiType() { return m_ApiType; }
            static Camera& GetMainCamera() { return m_Cameras[0]; }

            //TEMP: We'll expose a static method to swap the test texture
            static void SwapTestTexture();
            static Platform::IRenderingHardware& GetRHIRef() { return *m_RenderingHardware; };

        private:
            static std::shared_ptr<Resources::Texture2D> s_DefaultTexture;
            static std::shared_ptr<Resources::Texture2D> s_TestTexture;

            static Platform::RHI_API_TYPE m_ApiType;
            static Platform::IRenderingHardware* m_RenderingHardware;

            static uint32_t m_Width, m_Height;

            static std::vector<Camera> m_Cameras;
        };
    }
} // VoidArchitect
