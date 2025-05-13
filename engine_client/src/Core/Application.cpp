//
// Created by Michael Desmedt on 12/05/2025.
//
#include "Application.hpp"
#include "vapch.hpp"

#include "Events/ApplicationEvent.hpp"
#include "Logger.hpp"


namespace VoidArchitect
{
    Application::Application() = default;
    Application::~Application() = default;

    void Application::Run()
    {
        // TODO This is a temporary test
        WindowResizedEvent e(1280, 720);
        VA_ENGINE_TRACE(e.ToString());

        while (true)
            ;
    }
} // namespace VoidArchitect
