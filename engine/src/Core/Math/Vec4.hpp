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

    class Vec3;

    class Vec4
    {
        friend class Mat4;
        friend class Quat;

    public:
        static Vec4 Zero();
        static Vec4 One();

        static Vec4 FromString(const std::string& str);

        Vec4();
        Vec4(float x, float y, float z, float w);
        explicit Vec4(Vec3 vector, float w = 1.0f);

        Vec4 operator+(const Vec4& other) const;
        Vec4 operator-(const Vec4& other) const;
        Vec4 operator*(const Vec4& other) const;
        Vec4 operator/(const Vec4& other) const;
        Vec4 operator*(float scalar) const;
        Vec4 operator/(float scalar) const;
        Vec4& operator+=(const Vec4& other);
        Vec4& operator-=(const Vec4& other);

        void X(const float x) { m_Vector.x = x; }
        void Y(const float y) { m_Vector.y = y; }
        void Z(const float z) { m_Vector.z = z; }
        void W(const float w) { m_Vector.w = w; }

        [[nodiscard]] float X() const { return m_Vector.x; }
        [[nodiscard]] float Y() const { return m_Vector.y; }
        [[nodiscard]] float Z() const { return m_Vector.z; }
        [[nodiscard]] float W() const { return m_Vector.w; }
        bool operator==(const Vec4& Vec4) const = default;

    private:
        explicit Vec4(const impl::glm::vec4& vector);

        impl::glm::vec4 m_Vector;
    };
} // namespace VoidArchitect::Math
