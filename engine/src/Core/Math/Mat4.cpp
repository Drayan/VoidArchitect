//
// Created by Michael Desmedt on 20/05/2025.
//
#include "Mat4.hpp"

#include "Quat.hpp"
#include "Vec3.hpp"

namespace VoidArchitect::Math
{
    Mat4::Mat4()
        : m_Matrix()
    {
    }

    Mat4::Mat4(const impl::glm::mat4& matrix)
        : m_Matrix(matrix)
    {
    }

    Mat4 Mat4::Identity() { return Mat4(impl::glm::mat4(1.0f)); }

    Mat4 Mat4::Zero() { return Mat4(impl::glm::mat4(0.0f)); }

    Mat4 Mat4::Perspective(const float fov, const float aspect, const float near, const float far)
    {
        auto mat = Mat4(impl::glm::perspectiveRH_ZO(fov, aspect, near, far));
        return mat;
    }

    Mat4 Mat4::Orthographic(
        const float left,
        const float right,
        const float bottom,
        const float top,
        const float near,
        const float far)
    {
        auto mat = Mat4(impl::glm::orthoRH_ZO(left, right, bottom, top, near, far));
        return mat;
    }

    Mat4 Mat4::Translate(const float x, const float y, const float z)
    {
        return Mat4(impl::glm::translate(impl::glm::mat4(1.0f), impl::glm::vec3(x, y, z)));
    }

    Mat4 Mat4::Translate(const Vec3& translation)
    {
        return Mat4(impl::glm::translate(impl::glm::mat4(1.0f), translation.m_Vector));
    }

    Mat4 Mat4::Rotate(const float angle, const Vec3& axis)
    {
        return Mat4(impl::glm::rotate(impl::glm::mat4(1.0f), angle, axis.m_Vector));
    }

    Mat4 Mat4::Rotate(const float angle, const float x, const float y, const float z)
    {
        return Mat4(impl::glm::rotate(impl::glm::mat4(1.0f), angle, impl::glm::vec3(x, y, z)));
    }

    Mat4 Mat4::FromTRS(const Vec3& translation, const Quat& rotation, const Vec3& scale)
    {
        const auto scaleMat = Scale(scale.X(), scale.Y(), scale.Z());
        const auto rotMat = rotation.ToMat4();
        const auto transMat = Translate(translation.X(), translation.Y(), translation.Z());
        return transMat * rotMat * scaleMat;
    }

    Mat4 Mat4::FromQuaternion(const Quat& quat) { return quat.ToMat4(); }

    Mat4 Mat4::Scale(const float x, const float y, const float z)
    {
        return Mat4(impl::glm::scale(impl::glm::mat4(1.0f), impl::glm::vec3(x, y, z)));
    }

    Mat4 Mat4::Inverse(const Mat4& matrix) { return Mat4(impl::glm::inverse(matrix.m_Matrix)); }

    Mat4 Mat4::Transpose(const Mat4& matrix) { return Mat4(impl::glm::transpose(matrix.m_Matrix)); }

    Mat4 Mat4::operator*(const Mat4& other) const { return Mat4(m_Matrix * other.m_Matrix); }

    Mat4& Mat4::operator*=(const Mat4& other)
    {
        m_Matrix *= other.m_Matrix;
        return *this;
    }

    Mat4& Mat4::Inverse()
    {
        m_Matrix = impl::glm::inverse(m_Matrix);
        return *this;
    }

    bool Mat4::ToTRS(Vec3& translation, Quat& rotation, Vec3& scale) const
    {
        impl::glm::vec3 skew;
        impl::glm::vec4 perspective;

        const bool success = impl::glm::decompose(
            m_Matrix,
            scale.m_Vector,
            rotation.m_Quaternion,
            translation.m_Vector,
            skew,
            perspective);

        return success;
    }

    Vec3 Mat4::GetTranslation() const
    {
        return {m_Matrix[3][0], m_Matrix[3][1], m_Matrix[3][2]};
    }

    Quat Mat4::GetRotation() const
    {
        const auto scale = GetScale();
        impl::glm::mat3 rotMat;
        rotMat[0] = m_Matrix[0] / scale.X();
        rotMat[1] = m_Matrix[1] / scale.Y();
        rotMat[2] = m_Matrix[2] / scale.Z();
        return Quat(impl::glm::quat_cast(rotMat));
    }

    Vec3 Mat4::GetScale() const
    {
        return {
            impl::glm::length(m_Matrix[0]),
            impl::glm::length(m_Matrix[1]),
            impl::glm::length(m_Matrix[2])
        };
    }
} // namespace VoidArchitect::Math
