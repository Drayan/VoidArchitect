//
// Created by Michael Desmedt on 13/05/2025.
//
#pragma once
#include "Core/Window.hpp"

#include <SDL3/SDL.h>

namespace VoidArchitect::Platform
{
    class SDLWindow : public Window
    {
    public:
        SDLWindow(const WindowProps& props);
        virtual ~SDLWindow();

        void OnUpdate() override;

        unsigned int GetWidth() const override
        {
            if (!m_Window)
                return 0;

            int width;
            SDL_GetWindowSizeInPixels(m_Window, &width, nullptr);
            return static_cast<unsigned int>(width);
        };
        unsigned int GetHeight() const override
        {
            if (!m_Window)
                return 0;

            int height;
            SDL_GetWindowSizeInPixels(m_Window, nullptr, &height);
            return static_cast<unsigned int>(height);
        }

        void SetEventCallback(const EventCallbackFn& callback) override {}
        void SetVSync(bool enabled) override { SDL_SetWindowSurfaceVSync(m_Window, enabled); }
        bool IsVSync() const override
        {
            int enabled;
            SDL_GetWindowSurfaceVSync(m_Window, &enabled);
            return enabled > 0;
        }

    private:
        virtual void Initialize(const WindowProps& props);
        virtual void Shutdown();

    private:
        SDL_Window* m_Window;
    };
} // namespace VoidArchitect::Platform
