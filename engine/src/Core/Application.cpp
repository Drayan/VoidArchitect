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

#include "Systems/Renderer/Camera.hpp"

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
            Renderer::RenderCommand::Initialize(Platform::RHI_API_TYPE::Vulkan, m_MainWindow);
        }
        catch (std::exception& e)
        {
            VA_ENGINE_CRITICAL("[Application] Failed to initialize subsystem: {0}", e.what());
            exit(-1);
        }
    }

    Application::~Application() { Renderer::RenderCommand::Shutdown(); }

    void Application::Run()
    {
        while (m_Running)
        {
            for (Layer* layer : m_LayerStack)
                layer->OnUpdate(1.0f / 60.0f);

            m_MainWindow->OnUpdate();

            if (Renderer::RenderCommand::BeginFrame(1.0f / 60.0f))
            {
                Renderer::RenderCommand::EndFrame(1.0f / 60.0f);
            }
        }
    }

    void Application::OnEvent(Event& e)
    {
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<WindowCloseEvent>(BIND_EVENT_FN(OnWindowClose));
        dispatcher.Dispatch<WindowResizedEvent>(BIND_EVENT_FN(OnWindowResized));
        // TEMP This should not stay here; it's just a convenience to hit ESC to quit the app for
        // now.
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
        Renderer::RenderCommand::Resize(e.GetWidth(), e.GetHeight());
        return true;
    }

    bool Application::OnKeyPressed(KeyPressedEvent& e)
    {
        switch (e.GetKeyCode())
        {
            case SDLK_ESCAPE:
                m_Running = false;
                return true;

            default:
                break;
        }

        return false;
    }
} // namespace VoidArchitect
