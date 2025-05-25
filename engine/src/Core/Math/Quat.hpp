//
// Created by Michael Desmedt on 21/05/2025.
//
#pragma once
#include "Vec3.hpp"

namespace VoidArchitect::Math
{
    class Mat4;

    namespace impl
    {
#include <glm/gtc/quaternion.hpp>
    }

    class Quat
    {
    public:
        static Quat Identity();

        Quat();
        Quat(float x, float y, float z, float w);
        Quat(const Vec3& axis, float angle);

        Quat operator*(const Quat& other) const;
        Quat& operator*=(const Quat& other);

        static Quat FromEuler(float x, float y, float z);
        Vec3 ToEuler() const;

        Vec3 RotateVector(const Vec3& vector) const;

        Quat Normalize() const;

        Mat4 ToMat4() const;

    private:
        explicit Quat(const impl::glm::quat& quaternion);

        impl::glm::quat m_Quaternion;
    };
} // namespace VoidArchitect::Math
