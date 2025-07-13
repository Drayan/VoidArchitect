//
// Created by Michael Desmedt on 21/05/2025.
//
#include "DebugCameraController.hpp"

#include <VoidArchitect/Engine/Common/Logger.hpp>
#include <VoidArchitect/Engine/Common/Events/KeyEvent.hpp>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_mouse.h>

#include "Camera.hpp"

namespace VoidArchitect::Renderer
{
#define BIND_EVENT_FN(x) [this](auto&& PH1) { return this->x(std::forward<decltype(PH1)>(PH1)); }

    DebugCameraController::DebugCameraController(Camera& camera)
        : m_Camera(camera),
          m_CameraPosition(camera.GetPosition()),
          m_CameraOrientation(camera.GetRotation())
    {
        m_Forward = Math::Vec3::Forward();
        m_Right = Math::Vec3::Right();
        m_Up = Math::Vec3::Up();

        const auto euler = m_CameraOrientation.ToEuler();
        m_Pitch = euler.X();
        m_Yaw = euler.Y();
    }

    void DebugCameraController::UpdateCameraVectors()
    {
        m_Forward = m_CameraOrientation.RotateVector(Math::Vec3::Forward());
        m_Right = m_CameraOrientation.RotateVector(Math::Vec3::Right());
        m_Up = m_CameraOrientation.RotateVector(Math::Vec3::Up());
    }

    void DebugCameraController::OnFixedUpdate(const float fixedTimestep)
    {
        m_CameraPosition = m_Camera.GetPosition();

        Math::Vec3 velocity;

        if (m_MoveForward) velocity += m_Forward;
        if (m_MoveBackward) velocity -= m_Forward;
        if (m_MoveLeft) velocity -= m_Right;
        if (m_MoveRight) velocity += m_Right;
        if (m_MoveUp) velocity += m_Up;
        if (m_MoveDown) velocity -= m_Up;

        if (!velocity.IsZero()) velocity.Normalize();

        m_CameraPosition += velocity * m_MovementSpeed * fixedTimestep;
        m_Camera.SetPosition(m_CameraPosition);
        m_Camera.SetRotation(m_CameraOrientation);
    }

    bool DebugCameraController::OnMouseMoved(MouseMovedEvent& e)
    {
        static bool firstMouse = true;
        static float lastX;
        static float lastY;

        if (firstMouse)
        {
            lastX = e.GetX();
            lastY = e.GetY();
            firstMouse = false;
            return false;
        }

        float xOffset = lastX - e.GetX();
        float yOffset = lastY - e.GetY();

        lastX = e.GetX();
        lastY = e.GetY();

        if (m_MouseDrag)
        {
            m_Yaw += xOffset * m_RotationSpeed;
            m_Pitch += yOffset * m_RotationSpeed;

            constexpr auto pitchLimit = 87.f;
            m_Pitch = std::clamp(m_Pitch, -pitchLimit, pitchLimit);

            while (m_Yaw < -180.0f) m_Yaw += 360.0f;
            while (m_Yaw > 180.0f) m_Yaw -= 360.0f;

            m_CameraOrientation = Math::Quat::FromEuler(m_Pitch, m_Yaw, 0.0f);

            UpdateCameraVectors();
        }

        return false;
    }

    bool DebugCameraController::OnKeyPressed(KeyPressedEvent& e)
    {
        switch (e.GetKeyCode())
        {
            case SDLK_Z:
                m_MoveForward = true;
                break;
            case SDLK_S:
                m_MoveBackward = true;
                break;
            case SDLK_Q:
                m_MoveLeft = true;
                break;
            case SDLK_D:
                m_MoveRight = true;
                break;
            case SDLK_LSHIFT:
                m_MoveDown = true;
                break;
            case SDLK_SPACE:
                m_MoveUp = true;
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
                m_MoveForward = false;
                break;
            case SDLK_S:
                m_MoveBackward = false;
                break;
            case SDLK_Q:
                m_MoveLeft = false;
                break;
            case SDLK_D:
                m_MoveRight = false;
                break;
            case SDLK_LSHIFT:
                m_MoveDown = false;
                break;
            case SDLK_SPACE:
                m_MoveUp = false;
                break;
            default: ;
        }

        return false;
    }

    bool DebugCameraController::OnMouseButtonPressed(MouseButtonPressedEvent& e)
    {
        switch (e.GetMouseButton())
        {
            case SDL_BUTTON_RIGHT:
                m_MouseDrag = true;
                break;
            default: ;
        }

        return false;
    }

    bool DebugCameraController::OnMouseButtonReleased(MouseButtonReleasedEvent& e)
    {
        switch (e.GetMouseButton())
        {
            case SDL_BUTTON_RIGHT:
                m_MouseDrag = false;
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
        dispatcher.Dispatch<MouseMovedEvent>(BIND_EVENT_FN(OnMouseMoved));
        dispatcher.Dispatch<MouseButtonPressedEvent>(BIND_EVENT_FN(OnMouseButtonPressed));
        dispatcher.Dispatch<MouseButtonReleasedEvent>(BIND_EVENT_FN(OnMouseButtonReleased));
    }
} // namespace VoidArchitect::Renderer
