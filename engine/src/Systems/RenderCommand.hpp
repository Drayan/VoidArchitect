//
// Created by Michael Desmedt on 14/05/2025.
//
#pragma once

namespace VoidArchitect
{
    class Window;
}

namespace VoidArchitect::Platform
{
    enum class RHI_API_TYPE;
}

namespace VoidArchitect::Platform
{
    class IRenderingHardware;
}

namespace VoidArchitect
{
    class RenderCommand
    {
    public:
        static void Initialize(Platform::RHI_API_TYPE apiType, std::unique_ptr<Window>& window);
        static void Shutdown();

        static void Resize(uint32_t width, uint32_t height);

        static bool BeginFrame(float deltaTime);
        static bool EndFrame(float deltaTime);

    private:
        static Platform::IRenderingHardware* m_RenderingHardware;
    };
} // VoidArchitect
