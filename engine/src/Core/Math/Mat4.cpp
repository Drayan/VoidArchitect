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
        return Mat4(impl::glm::orthoRH_ZO(left, right, bottom, top, near, far));
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
} // namespace VoidArchitect::Math
