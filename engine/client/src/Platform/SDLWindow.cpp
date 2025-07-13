//
// Created by Michael Desmedt on 13/05/2025.
//
#include "SDLWindow.hpp"

#include <VoidArchitect/Engine/Common/Events/ApplicationEvent.hpp>
#include <VoidArchitect/Engine/Common/Events/KeyEvent.hpp>
#include <VoidArchitect/Engine/Common/Events/MouseEvent.hpp>
#include <VoidArchitect/Engine/Common/Logger.hpp>

#include <SDL3/SDL_vulkan.h>

#include "SDLSurfaceFactory.hpp"

namespace VoidArchitect
{
    Window* Window::Create(const WindowProps& props) { return new Platform::SDLWindow(props); }

    namespace Platform
    {
        static bool s_IsSDLInitialized = false;

        SDLWindow::SDLWindow(const WindowProps& props) { SDLWindow::Initialize(props); }

        SDLWindow::~SDLWindow() { SDLWindow::Shutdown(); }

        void SDLWindow::Initialize(const WindowProps& props)
        {
            if (!s_IsSDLInitialized)
            {
                const bool result = SDL_Init(SDL_INIT_VIDEO);
                VA_ENGINE_ASSERT(result, "Could not initialize SDL!");
                s_IsSDLInitialized = true;
            }

            // Create window with Vulkan support
            m_Window = SDL_CreateWindow(
                props.Title.c_str(),
                props.Width,
                props.Height,
                SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);

            if (!m_Window)
            {
                VA_ENGINE_CRITICAL("Failed to create SDL window: {}", SDL_GetError());
                return;
            }

            SDL_ShowWindow(m_Window);
        }

        void SDLWindow::Shutdown()
        {
            SDL_DestroyWindow(m_Window);
            m_Window = nullptr;
        }

        std::unique_ptr<RHI::ISurfaceFactory> SDLWindow::CreateSurfaceFactory()
        {
            return std::make_unique<SDLSurfaceFactory>(m_Window);
        }

        void SDLWindow::OnUpdate()
        {
            // NOTE The engine currently is single-threaded, it make sense to poll event
            //  as quickly as possible here, but we should probably switch to SDL_WaitEvent in
            //  a multi-threaded scenario.
            SDL_Event e;
            while (SDL_PollEvent(&e))
            {
                switch (e.type)
                {
                    case SDL_EVENT_QUIT:
                    {
                        auto closeEvent = WindowCloseEvent();
                        m_EventCallback(closeEvent);
                        break;
                    }

                    // --- Window events ---
                    case SDL_EVENT_WINDOW_RESIZED:
                    {
                        auto resizeEvent = WindowResizedEvent(e.window.data1, e.window.data2);
                        m_EventCallback(resizeEvent);
                        break;
                    }

                    // --- Keyboard events ---
                    // TODO Translate SDL_Keycode to Engine's keycode.
                    case SDL_EVENT_KEY_DOWN:
                    {
                        auto keyEvent = KeyPressedEvent(e.key.key, e.key.repeat);
                        m_EventCallback(keyEvent);
                        break;
                    }

                    case SDL_EVENT_KEY_UP:
                    {
                        auto keyEvent = KeyReleasedEvent(e.key.key);
                        m_EventCallback(keyEvent);
                        break;
                    }

                    // --- Mouse events ---
                    case SDL_EVENT_MOUSE_MOTION:
                    {
                        auto mouseEvent = MouseMovedEvent(e.motion.x, e.motion.y);
                        m_EventCallback(mouseEvent);
                        break;
                    }

                    case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    {
                        auto mouseEvent = MouseButtonPressedEvent(
                            e.button.x,
                            e.button.y,
                            e.button.button);
                        m_EventCallback(mouseEvent);
                        break;
                    }

                    case SDL_EVENT_MOUSE_BUTTON_UP:
                    {
                        auto mouseEvent = MouseButtonReleasedEvent(
                            e.button.x,
                            e.button.y,
                            e.button.button);
                        m_EventCallback(mouseEvent);
                        break;
                    }

                    case SDL_EVENT_MOUSE_WHEEL:
                    {
                        auto mouseEvent = MouseScrolledEvent(
                            e.wheel.mouse_x,
                            e.wheel.mouse_y,
                            e.wheel.x,
                            e.wheel.y);
                        m_EventCallback(mouseEvent);
                        break;
                    }
                    default:
                        break;
                }
            }
        }
    } // namespace Platform
} // namespace VoidArchitect
