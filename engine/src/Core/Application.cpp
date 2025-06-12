//
// Created by Michael Desmedt on 12/05/2025.
//
#include "Application.hpp"

#include "../Systems/Renderer/RenderCommand.hpp"
#include "Events/ApplicationEvent.hpp"
#include "Events/KeyEvent.hpp"
#include "Logger.hpp"
#include "Platform/RHI/IRenderingHardware.hpp"
#include "Window.hpp"

// TEMP Remove this include when we have proper keycode.
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_timer.h>

#include "Systems/Renderer/RenderSystem.hpp"
#include "Systems/ResourceSystem.hpp"

namespace VoidArchitect
{
#define BIND_EVENT_FN(x) [this](auto&& PH1) { return this->x(std::forward<decltype(PH1)>(PH1)); }

    Application::Application()
    {
        m_MainWindow = std::unique_ptr<Window>(Window::Create());
        m_MainWindow->SetEventCallback(BIND_EVENT_FN(OnEvent));

        // Setting up Subsystems
        try
        {
            g_ResourceSystem = std::make_unique<ResourceSystem>();
            Renderer::g_RenderSystem = std::make_unique<Renderer::RenderSystem>(
                Platform::RHI_API_TYPE::Vulkan, m_MainWindow);

            Renderer::g_RenderSystem->InitializeSubsystems();
        }
        catch (std::exception& e)
        {
            VA_ENGINE_CRITICAL("[Application] Failed to initialize subsystem: {0}", e.what());
            exit(-1);
        }
    }

    Application::~Application()
    {
        Renderer::g_RenderSystem = nullptr;
        g_ResourceSystem = nullptr;
    }

    void Application::Run()
    {
        constexpr double FIXED_STEP = 1.0 / 60.0;
        double accumulator = 0.0;
        double currentTime = SDL_GetTicks() / 1000.f;
        while (m_Running)
        {
            const double newTime = SDL_GetTicks() / 1000.f;
            const double frameTime = newTime - currentTime;
            currentTime = newTime;

            accumulator += frameTime;

            while (accumulator >= FIXED_STEP)
            {
                for (Layer* layer : m_LayerStack)
                    layer->OnFixedUpdate(FIXED_STEP);

                accumulator -= FIXED_STEP;
            }

            Renderer::g_RenderSystem->RenderFrame(frameTime);

            m_MainWindow->OnUpdate();
        }
    }

    void Application::OnEvent(Event& e)
    {
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<WindowCloseEvent>(BIND_EVENT_FN(OnWindowClose));
        dispatcher.Dispatch<WindowResizedEvent>(BIND_EVENT_FN(OnWindowResized));
        // TEMP This should not stay here; it's just a convenience to hit ESC to quit the app for
        //  now.
        dispatcher.Dispatch<KeyPressedEvent>(BIND_EVENT_FN(OnKeyPressed));

        // Going through the layers backwards to propagate the event.
        for (auto it = m_LayerStack.end(); it != m_LayerStack.begin();)
        {
            (*--it)->OnEvent(e);
            if (e.Handled)
                break;
        }
    }

    void Application::PushLayer(Layer* layer)
    {
        m_LayerStack.PushLayer(layer);
        layer->OnAttach();
    }

    void Application::PushOverlay(Layer* layer)
    {
        m_LayerStack.PushOverlay(layer);
        layer->OnAttach();
    }

    bool Application::OnWindowClose(WindowCloseEvent& e)
    {
        m_Running = false;
        return true;
    }

    bool Application::OnWindowResized(WindowResizedEvent& e)
    {
        VA_ENGINE_TRACE("[Application] Window resized to {0}, {1}.", e.GetWidth(), e.GetHeight());
        Renderer::g_RenderSystem->Resize(e.GetWidth(), e.GetHeight());
        return true;
    }

    bool Application::OnKeyPressed(KeyPressedEvent& e)
    {
        switch (e.GetKeyCode())
        {
            case SDLK_ESCAPE:
                m_Running = false;
                return true;

            // Debug rendering modes
            case SDLK_0:
                Renderer::g_RenderSystem->SetDebugMode(Renderer::RenderSystemDebugMode::None);
                return true;
            case SDLK_1:
                Renderer::g_RenderSystem->SetDebugMode(Renderer::RenderSystemDebugMode::Lighting);
                return true;
            case SDLK_2:
                Renderer::g_RenderSystem->SetDebugMode(Renderer::RenderSystemDebugMode::Normals);
                return true;

            default:
                break;
        }

        return false;
    }
} // namespace VoidArchitect
