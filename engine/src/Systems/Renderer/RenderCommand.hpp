//
// Created by Michael Desmedt on 14/05/2025.
//
#pragma once

namespace VoidArchitect
{
    class Window;

    namespace Platform
    {
        enum class RHI_API_TYPE;
        class IRenderingHardware;
    }

    namespace Renderer
    {
        class Camera;

        class RenderCommand
        {
        public:
            static void Initialize(Platform::RHI_API_TYPE apiType, std::unique_ptr<Window>& window);
            static void Shutdown();

            static void Resize(uint32_t width, uint32_t height);

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

        private:
            static Platform::IRenderingHardware* m_RenderingHardware;

            static uint32_t m_Width, m_Height;

            static std::vector<Camera> m_Cameras;
        };
    }
} // VoidArchitect
