//
// Created by Michael Desmedt on 21/05/2025.
//
#include "DebugCameraController.hpp"

#include "Core/Events/KeyEvent.hpp"
#include <SDL3/SDL_keycode.h>

#include "Camera.hpp"
#include "Core/Logger.hpp"

namespace VoidArchitect::Renderer
{
#define BIND_EVENT_FN(x) [this](auto && PH1) { return this->x(std::forward<decltype(PH1)>(PH1)); }

    DebugCameraController::DebugCameraController(Camera& camera)
        : m_Camera(camera)
    {
    }

    void DebugCameraController::OnUpdate(const float deltaTime)
    {
        const auto position = m_Camera.GetPosition();
        m_Camera.SetPosition(position + m_CameraPosition * deltaTime * 2.f);
    }

    bool DebugCameraController::OnKeyPressed(KeyPressedEvent& e)
    {
        switch (e.GetKeyCode())
        {
            case SDLK_Z:
                m_CameraPosition.Z(-1.f);
                break;
            case SDLK_S:
                m_CameraPosition.Z(1.f);
                break;
            case SDLK_Q:
                m_CameraPosition.X(-1.f);
                break;
            case SDLK_D:
                m_CameraPosition.X(1.f);
                break;
            case SDLK_LSHIFT:
                m_CameraPosition.Y(-1.f);
                break;
            case SDLK_SPACE:
                m_CameraPosition.Y(1.f);
                break;
            default: ;
        }

        return false;
    }

    bool DebugCameraController::OnKeyReleased(KeyReleasedEvent& e)
    {
        switch (e.GetKeyCode())
        {
            case SDLK_Z:
            case SDLK_S:
                m_CameraPosition.Z(0.f);
                break;
            case SDLK_Q:
            case SDLK_D:
                m_CameraPosition.X(0.f);
                break;
            case SDLK_LSHIFT:
            case SDLK_SPACE:
                m_CameraPosition.Y(0.f);
                break;
            default: ;
        }

        return false;
    }

    void DebugCameraController::OnEvent(Event& e)
    {
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<KeyPressedEvent>(BIND_EVENT_FN(OnKeyPressed));
        dispatcher.Dispatch<KeyReleasedEvent>(BIND_EVENT_FN(OnKeyReleased));
    }
}
