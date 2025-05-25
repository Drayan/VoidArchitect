//
// Created by Michael Desmedt on 20/05/2025.
//
#pragma once
#include "Core/Math/Mat4.hpp"
#include "Core/Math/Quat.hpp"
#include "Core/Math/Vec3.hpp"

namespace VoidArchitect::Renderer
{
    enum class CameraType
    {
        Perspective,
        Orthographic,
    };

    class Camera
    {
    public:
        Camera(float fov, float aspect, float near, float far);
        Camera(float top, float bottom, float left, float right, float near, float far);

        void RecalculateView();

        Math::Mat4& GetProjection() { return m_Projection; }
        Math::Mat4& GetView() { return m_View; }

        Math::Vec3 GetPosition() const { return m_Position; }

        void SetAspectRatio(const float aspectRatio) { m_AspectRatio = aspectRatio; }
        void SetPosition(const Math::Vec3& position) { m_Position = position; }

        void SetPosition(const float x, const float y, const float z)
        {
            m_Position = Math::Vec3(x, y, z);
        }

        void SetRotation(const Math::Quat& quat) { m_Rotation = quat; }

        Math::Quat GetRotation() const { return m_Rotation; }

    private:
        CameraType m_Type;

        float m_Fov;
        float m_AspectRatio;

        float m_Top;
        float m_Bottom;
        float m_Left;
        float m_Right;
        float m_Near;
        float m_Far;

        Math::Mat4 m_View;
        Math::Mat4 m_Projection;

        Math::Vec3 m_Position;
        Math::Quat m_Rotation;
    };
} // namespace VoidArchitect::Renderer
