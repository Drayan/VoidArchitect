//
// Created by Michael Desmedt on 22/05/2025.
//
#pragma once

namespace VoidArchitect::Math
{
    namespace impl
    {
#include <glm/vec4.hpp>
    }

    class Vec4
    {
        friend class Mat4;
        friend class Quat;

    public:
        static Vec4 Zero();
        static Vec4 One();

        Vec4();
        Vec4(float x, float y, float z, float w);

        Vec4 operator+(const Vec4& other) const;
        Vec4 operator-(const Vec4& other) const;
        Vec4 operator*(const Vec4& other) const;
        Vec4 operator/(const Vec4& other) const;
        Vec4 operator*(const float scalar) const;
        Vec4 operator/(const float scalar) const;
        Vec4& operator+=(const Vec4& other);
        Vec4& operator-=(const Vec4& other);

        void X(const float x) { m_Vector.x = x; }
        void Y(const float y) { m_Vector.y = y; }
        void Z(const float z) { m_Vector.z = z; }
        void W(const float w) { m_Vector.w = w; }

        float X() const { return m_Vector.x; }
        float Y() const { return m_Vector.y; }
        float Z() const { return m_Vector.z; }
        float W() const { return m_Vector.w; }

    private:
        explicit Vec4(const impl::glm::vec4& vector);

        impl::glm::vec4 m_Vector;
    };
}
