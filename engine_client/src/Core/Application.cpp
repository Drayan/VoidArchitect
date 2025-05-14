//
// Created by Michael Desmedt on 12/05/2025.
//
#include "Application.hpp"

#include "Events/ApplicationEvent.hpp"
#include "Logger.hpp"
#include "Window.hpp"

namespace VoidArchitect
{
#define BIND_EVENT_FN(x) std::bind(&Application::x, this, std::placeholders::_1)

    Application::Application()
    {
        m_MainWindow = std::unique_ptr<Window>(Window::Create());
        m_MainWindow->SetEventCallback(BIND_EVENT_FN(OnEvent));
    }
    Application::~Application() = default;

    void Application::Run()
    {
        while (m_Running)
        {
            m_MainWindow->OnUpdate();
        }
    }

    void Application::OnEvent(Event& e)
    {
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<WindowCloseEvent>(BIND_EVENT_FN(OnWindowClose));
    }

    bool Application::OnWindowClose(WindowCloseEvent& e)
    {
        m_Running = false;
        return true;
    }

} // namespace VoidArchitect
