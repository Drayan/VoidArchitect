//
// Created by Michael Desmedt on 07/06/2025.
//
#pragma once
#include "Camera.hpp"
#include "PassRenderers.hpp"
#include "RenderGraph.hpp"

namespace VoidArchitect
{
    class Window;

    namespace Platform
    {
        enum class RHI_API_TYPE;
        class IRenderingHardware;
    } // namespace Platform

    namespace Renderer
    {
        enum class RenderSystemDebugMode
        {
            None = 0,
            Lighting = 1,
            Normals = 2,
        };

        class RenderSystem
        {
        public:
            RenderSystem(Platform::RHI_API_TYPE apiType, std::unique_ptr<Window>& window);
            void InitializeSubsystems();
            ~RenderSystem();

            void RenderFrame(float frameTime);

            void Resize(uint32_t width, uint32_t height);

            std::unique_ptr<Platform::IRenderingHardware>& GetRHI() { return m_RHI; }
            // TEMP I will expose a camera from here, for debugging purpose until we get a proper
            //       scene management and we can remove this ugly hack.
            Camera& GetMainCamera() { return m_MainCamera; }

            void SetDebugMode(RenderSystemDebugMode mode) { m_DebugMode = mode; }

        private:
            Platform::RHI_API_TYPE m_ApiType;
            uint32_t m_Width, m_Height;

            std::unique_ptr<Platform::IRenderingHardware> m_RHI;

            RenderGraph m_RenderGraph;

            ForwardOpaquePassRenderer m_ForwardOpaquePassRenderer;
            UIPassRenderer m_UIPassRenderer;

            Camera m_MainCamera;
            RenderSystemDebugMode m_DebugMode = RenderSystemDebugMode::None;
        };

        inline std::unique_ptr<RenderSystem> g_RenderSystem;
    } // namespace Renderer
} // namespace VoidArchitect
