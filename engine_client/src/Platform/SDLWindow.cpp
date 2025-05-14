//
// Created by Michael Desmedt on 13/05/2025.
//
#include "SDLWindow.hpp"

#include "Core/Logger.hpp"

namespace VoidArchitect
{
    Window* Window::Create(const WindowProps& props) { return new Platform::SDLWindow(props); }

    namespace Platform
    {
        static bool s_IsSDLInitialized = false;


        SDLWindow::SDLWindow(const WindowProps& props) { SDLWindow::Initialize(props); }

        SDLWindow::~SDLWindow()
        {
            SDL_DestroyWindow(m_Window);
            m_Window = nullptr;
        }

        void SDLWindow::Initialize(const WindowProps& props)
        {
            if (!s_IsSDLInitialized)
            {
                const bool result = SDL_Init(SDL_INIT_VIDEO);
                VA_ENGINE_ASSERT(result, "Could not initialize SDL!");
                s_IsSDLInitialized = true;
            }

            m_Window = SDL_CreateWindow(
                props.Title.c_str(), props.Width, props.Height, SDL_WINDOW_RESIZABLE);

            SDL_SetWindowSurfaceVSync(m_Window, true);
        }

        void SDLWindow::Shutdown() {}

        void SDLWindow::OnUpdate()
        {
            SDL_Event e;
            while (SDL_PollEvent(&e))
            {
                switch (e.type)
                {
                    // TODO Add event handling here
                }
            }
        }
    } // namespace Platform
} // namespace VoidArchitect
