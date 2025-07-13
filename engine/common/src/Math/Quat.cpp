//
// Created by Michael Desmedt on 21/05/2025.
//
#include "Quat.hpp"

#include "Constants.hpp"
#include "Mat4.hpp"

namespace VoidArchitect::Math
{
    Quat::Quat() : m_Quaternion() {}

    Quat::Quat(const impl::glm::quat& quaternion) : m_Quaternion(quaternion) {}

    Quat::Quat(float x, float y, float z, float w) : m_Quaternion(w, x, y, z) {}

    Quat::Quat(const Vec3& axis, const float angle) : m_Quaternion(angle, axis.m_Vector) {}

    Quat Quat::FromEuler(const float x, const float y, const float z)
    {
        return Quat(impl::glm::quat(impl::glm::vec3(x, y, z)));
    }

    Vec3 Quat::ToEuler() const { return Vec3(impl::glm::eulerAngles(m_Quaternion)); }

    Vec3 Quat::RotateVector(const Vec3& vector) const
    {
        return Vec3(m_Quaternion * vector.m_Vector);
    }

    Quat Quat::Normalize() const { return Quat(impl::glm::normalize(m_Quaternion)); }

    Mat4 Quat::ToMat4() const
    {
        const auto rotMat = impl::glm::mat4_cast(m_Quaternion);
        return Mat4(rotMat);
    }

    Quat Quat::Identity() { return Quat(impl::glm::quat(1.0f, 0.0f, 0.0f, 0.0f)); }

    Quat Quat::operator*(const Quat& other) const
    {
        return Quat(m_Quaternion * other.m_Quaternion);
    }

    Quat& Quat::operator*=(const Quat& other)
    {
        m_Quaternion *= other.m_Quaternion;
        return *this;
    }
} // namespace VoidArchitect::Math
