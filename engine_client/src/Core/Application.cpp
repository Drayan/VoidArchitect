//
// Created by Michael Desmedt on 12/05/2025.
//
#include "Application.hpp"

#include "Events/ApplicationEvent.hpp"
#include "Logger.hpp"
#include "Window.hpp"

namespace VoidArchitect
{
    Application::Application() { m_MainWindow = std::unique_ptr<Window>(Window::Create()); }
    Application::~Application() = default;

    void Application::Run()
    {
        while (m_Running)
        {
            m_MainWindow->OnUpdate();
        }
    }
} // namespace VoidArchitect
