//
// Created by Michael Desmedt on 20/05/2025.
//
#pragma once
#include "Vec4.hpp"

namespace VoidArchitect::Math
{
    class Vec3;
    class Quat;

    namespace impl
    {
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/matrix_decompose.hpp>
    } // namespace impl

    class Mat4
    {
        friend class Vec3;
        friend class Quat;

    public:
        Mat4();
        Mat4(
            float m00,
            float m01,
            float m02,
            float m03,
            float m10,
            float m11,
            float m12,
            float m13,
            float m20,
            float m21,
            float m22,
            float m23,
            float m30,
            float m31,
            float m32,
            float m33);

        static Mat4 Identity();
        static Mat4 Zero();
        static Mat4 Perspective(float fov, float aspect, float near, float far);
        static Mat4 Orthographic(
            float left,
            float right,
            float bottom,
            float top,
            float near,
            float far);
        static Mat4 Translate(float x, float y, float z);
        static Mat4 Translate(const Vec3& translation);
        static Mat4 Rotate(float angle, const Vec3& axis);
        static Mat4 Rotate(float angle, float x, float y, float z);
        static Mat4 FromQuaternion(const Quat& quat);
        static Mat4 Scale(float x, float y, float z);
        static Mat4 Inverse(const Mat4& matrix);
        static Mat4 Transpose(const Mat4& matrix);
        static Mat4 FromTRS(const Vec3& translation, const Quat& rotation, const Vec3& scale);

        Mat4 operator*(const Mat4& other) const;
        Mat4& operator*=(const Mat4& other);

        Vec4 operator*(const Vec4& other) const;

        Mat4& Inverse();
        bool ToTRS(Vec3& translation, Quat& rotation, Vec3& scale) const;
        Vec3 GetTranslation() const;
        Quat GetRotation() const;
        Vec3 GetScale() const;

    private:
        explicit Mat4(const impl::glm::mat4& matrix);

        impl::glm::mat4 m_Matrix;
    };
} // namespace VoidArchitect::Math
