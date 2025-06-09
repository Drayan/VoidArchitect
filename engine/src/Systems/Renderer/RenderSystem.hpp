//
// Created by Michael Desmedt on 07/06/2025.
//
#pragma once
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
        class RenderSystem
        {
        public:
            RenderSystem(Platform::RHI_API_TYPE apiType, std::unique_ptr<Window>& window);
            void InitializeSubsystems();
            ~RenderSystem();

            void RenderFrame(float frameTime);

            std::unique_ptr<Platform::IRenderingHardware>& GetRHI() { return m_RHI; }

        private:
            Platform::RHI_API_TYPE m_ApiType;
            uint32_t m_Width, m_Height;

            std::unique_ptr<Platform::IRenderingHardware> m_RHI;

            RenderGraph m_RenderGraph;

            ForwardOpaquePassRenderer m_ForwardOpaquePassRenderer;
            UIPassRenderer m_UIPassRenderer;
        };

        inline std::unique_ptr<RenderSystem> g_RenderSystem;
    } // Renderer
} // VoidArchitect
