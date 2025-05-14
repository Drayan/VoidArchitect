//
// Created by Michael Desmedt on 12/05/2025.
//
#pragma once

#include "Core.hpp"
#include "Events/ApplicationEvent.hpp"
#include "Events/Event.hpp"

#include <memory>

namespace VoidArchitect
{
    class Window;

    class VA_API Application
    {
    public:
        Application();
        virtual ~Application();

        void Run();

        void OnEvent(Event& e);

    private:
        bool OnWindowClose(WindowCloseEvent& e);

        std::unique_ptr<Window> m_MainWindow;
        bool m_Running = true;
    };

    Application* CreateApplication();
} // namespace VoidArchitect
