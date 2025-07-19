//
// Created by Michael Desmedt on 21/05/2025.
//
#pragma once
#include <VoidArchitect/Engine/Common/Math/Quat.hpp>
#include <VoidArchitect/Engine/Common/Math/Vec3.hpp>
#include <VoidArchitect/Engine/Common/Systems/Events/InputEvents.hpp>

namespace VoidArchitect ::Renderer
{
    class Camera;

    class DebugCameraController
    {
    public:
        DebugCameraController(Camera& camera);

        void OnFixedUpdate(float fixedTimestep);

        bool OnKeyPressed(const Events::KeyPressedEvent& e);
        bool OnKeyReleased(const Events::KeyReleasedEvent& e);
        bool OnMouseMoved(const Events::MouseMovedEvent& e);
        bool OnMouseButtonPressed(const Events::MouseButtonPressedEvent& e);
        bool OnMouseButtonReleased(const Events::MouseButtonReleasedEvent& e);

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

        float m_MovementSpeed = 5.f;
        float m_RotationSpeed = .005f;

        bool m_MoveForward = false;
        bool m_MoveBackward = false;
        bool m_MoveLeft = false;
        bool m_MoveRight = false;
        bool m_MoveUp = false;
        bool m_MoveDown = false;

        bool m_MouseDrag = false;

        // Events subscriptions
        Events::EventSubscription m_KeyPressedSub;
        Events::EventSubscription m_KeyReleasedSub;
        Events::EventSubscription m_MouseMovedSub;
        Events::EventSubscription m_MouseButtonPressedSub;
        Events::EventSubscription m_MouseButtonReleasedSub;
    };
} // namespace VoidArchitect::Renderer
