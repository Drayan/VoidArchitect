//
// Created by Michael Desmedt on 21/05/2025.
//
#pragma once
#include "Core/Events/Event.hpp"
#include "Core/Events/KeyEvent.hpp"
#include "Core/Events/MouseEvent.hpp"
#include "Core/Math/Quat.hpp"
#include "Core/Math/Vec3.hpp"

namespace VoidArchitect ::Renderer
{
    class Camera;

    class DebugCameraController
    {
    public:
        DebugCameraController(Camera& camera);

        void OnUpdate(float deltaTime);
        void OnEvent(Event& e);

        bool OnKeyPressed(KeyPressedEvent& e);
        bool OnKeyReleased(KeyReleasedEvent& e);
        bool OnMouseMoved(MouseMovedEvent& e);
        bool OnMouseButtonPressed(MouseButtonPressedEvent& e);
        bool OnMouseButtonReleased(MouseButtonReleasedEvent& e);

    private:
        void UpdateCameraVectors();

        Camera& m_Camera;

        Math::Vec3 m_CameraPosition;
        Math::Quat m_CameraOrientation;

        Math::Vec3 m_Forward;
        Math::Vec3 m_Right;
        Math::Vec3 m_Up;

        float m_Pitch = 0.0f;
        float m_Yaw = 0.0f;

        float m_MovementSpeed = 5.0f;
        float m_RotationSpeed = .005f;

        bool m_MoveForward = false;
        bool m_MoveBackward = false;
        bool m_MoveLeft = false;
        bool m_MoveRight = false;
        bool m_MoveUp = false;
        bool m_MoveDown = false;

        bool m_MouseDrag = false;
    };
}
