//
// Created by Michael Desmedt on 20/05/2025.
//
#pragma once

namespace VoidArchitect::Math
{
    class Vec3;
    class Quat;

    namespace impl
    {
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
    } // namespace impl

    class Mat4
    {
        friend class Vec3;
        friend class Quat;

    public:
        Mat4();

        static Mat4 Identity();
        static Mat4 Zero();
        static Mat4 Perspective(float fov, float aspect, float near, float far);
        static Mat4 Orthographic(
            float left, float right, float bottom, float top, float near, float far);
        static Mat4 Translate(float x, float y, float z);
        static Mat4 Translate(const Vec3& translation);
        static Mat4 Rotate(float angle, const Vec3& axis);
        static Mat4 Rotate(float angle, float x, float y, float z);
        static Mat4 FromQuaternion(const Quat& quat);
        static Mat4 Scale(float x, float y, float z);
        static Mat4 Inverse(const Mat4& matrix);
        static Mat4 Transpose(const Mat4& matrix);

        Mat4 operator*(const Mat4& other) const;
        Mat4& operator*=(const Mat4& other);

    private:
        explicit Mat4(const impl::glm::mat4& matrix);

        impl::glm::mat4 m_Matrix;
    };
} // namespace VoidArchitect::Math
