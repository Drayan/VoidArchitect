//
// Created by Michael Desmedt on 20/05/2025.
//
#include "Camera.hpp"

#include "Core/Math/Constants.hpp"

namespace VoidArchitect::Renderer
{
    Camera::Camera(const float fov, const float aspect, const float near, const float far)
        : m_Type(CameraType::Perspective),
          m_Fov(fov * Math::DEG2RAD),
          m_AspectRatio(aspect),
          m_Top(),
          m_Bottom(),
          m_Left(),
          m_Right(),
          m_Near(near),
          m_Far(far)
    {
        RecalculateView();
    }

    Camera::Camera(
        const float top,
        const float bottom,
        const float left,
        const float right,
        const float near,
        const float far)
        : m_Type(CameraType::Orthographic),
          m_Fov(),
          m_AspectRatio(),
          m_Top(top),
          m_Bottom(bottom),
          m_Left(left),
          m_Right(right),
          m_Near(near),
          m_Far(far)
    {
        RecalculateView();
    }

    void Camera::RecalculateView()
    {
        switch (m_Type)
        {
            case CameraType::Perspective:
                m_Projection = Math::Mat4::Perspective(m_Fov, m_AspectRatio, m_Near, m_Far);
                break;

            case CameraType::Orthographic:
                m_Projection = Math::Mat4::Orthographic(
                    m_Left,
                    m_Right,
                    m_Bottom,
                    m_Top,
                    m_Near,
                    m_Far);
                break;
        }
        m_View = Math::Mat4::Inverse(
            Math::Mat4::Translate(m_Position) * Math::Mat4::FromQuaternion(m_Rotation));
    }
} // VoidArchitect
