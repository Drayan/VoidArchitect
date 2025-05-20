//
// Created by Michael Desmedt on 21/05/2025.
//
#pragma once
#include "Core/Events/Event.hpp"
#include "Core/Events/KeyEvent.hpp"
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

    private:
        Camera& m_Camera;

        Math::Vec3 m_CameraPosition;
    };
}
