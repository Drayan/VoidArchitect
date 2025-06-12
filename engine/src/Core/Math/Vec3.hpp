//
// Created by Michael Desmedt on 20/05/2025.
//
#pragma once

namespace VoidArchitect::Math
{
    namespace impl
    {
#include <glm/geometric.hpp>
#include <glm/vec3.hpp>
    } // namespace impl

    class Vec3
    {
        friend class Mat4;
        friend class Quat;

    public:
        static Vec3 Zero();
        static Vec3 One();
        static Vec3 Left();
        static Vec3 Right();
        static Vec3 Up();
        static Vec3 Down();
        static Vec3 Forward();
        static Vec3 Back();

        static Vec3 Cross(const Vec3& v1, const Vec3& v2);

        Vec3();
        Vec3(float x, float y, float z);

        void Normalize();
        bool IsZero() const;

        Vec3 operator+(const Vec3& other) const;
        Vec3 operator-(const Vec3& other) const;
        Vec3 operator*(const Vec3& other) const;
        Vec3 operator/(const Vec3& other) const;
        Vec3 operator*(float scalar) const;
        Vec3 operator/(float scalar) const;
        Vec3& operator*=(float scalar);
        Vec3& operator+=(const Vec3& other);
        Vec3& operator-=(const Vec3& other);
        Vec3& operator-();

        void X(const float x) { m_Vector.x = x; }
        void Y(const float y) { m_Vector.y = y; }
        void Z(const float z) { m_Vector.z = z; }

        float X() const { return m_Vector.x; }
        float Y() const { return m_Vector.y; }
        float Z() const { return m_Vector.z; }

    private:
        explicit Vec3(const impl::glm::vec3& vector);

        impl::glm::vec3 m_Vector;
    };
} // namespace VoidArchitect::Math
