//
// Created by Michael Desmedt on 12/05/2025.
//
#pragma once

#include "Core.hpp"
#include "LayerStack.hpp"

// NOTE Currently the client application doesn't use PCH, therefore it need memory.
#include "Events/KeyEvent.hpp"

#include <memory>

namespace VoidArchitect::Platform
{
    class IThread;
}

namespace VoidArchitect
{
    class Event;
    class WindowCloseEvent;
    class WindowResizedEvent;
    class Window;

    class VA_API Application
    {
    public:
        Application();
        virtual ~Application();

        void Run();

        void OnEvent(Event& e);

        void PushLayer(Layer* layer);
        void PushOverlay(Layer* layer);

    private:
        bool OnWindowClose(WindowCloseEvent& e);
        bool OnWindowResized(WindowResizedEvent& e);
        bool OnKeyPressed(KeyPressedEvent& e);

        std::unique_ptr<Window> m_MainWindow;
        bool m_Running = true;

        std::unique_ptr<Platform::IThread> m_TestThread;

        LayerStack m_LayerStack;
    };

    Application* CreateApplication();
} // namespace VoidArchitect
