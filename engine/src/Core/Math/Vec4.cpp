//
// Created by Michael Desmedt on 22/05/2025.
//
#include "Vec4.hpp"

namespace VoidArchitect::Math
{
    Vec4::Vec4() : m_Vector() {}

    Vec4::Vec4(const impl::glm::vec4& vector) : m_Vector(vector) {}

    Vec4::Vec4(const float x, const float y, const float z, const float w) : m_Vector(x, y, z, w) {}

    Vec4 Vec4::operator+(const Vec4& other) const { return Vec4(m_Vector + other.m_Vector); }

    Vec4 Vec4::operator-(const Vec4& other) const { return Vec4(m_Vector - other.m_Vector); }

    Vec4 Vec4::operator*(const Vec4& other) const { return Vec4(m_Vector * other.m_Vector); }

    Vec4 Vec4::operator/(const Vec4& other) const { return Vec4(m_Vector / other.m_Vector); }

    Vec4 Vec4::operator*(const float scalar) const { return Vec4(m_Vector * scalar); }

    Vec4 Vec4::operator/(const float scalar) const { return Vec4(m_Vector / scalar); }

    Vec4& Vec4::operator+=(const Vec4& other)
    {
        m_Vector += other.m_Vector;
        return *this;
    }

    Vec4& Vec4::operator-=(const Vec4& other)
    {
        m_Vector -= other.m_Vector;
        return *this;
    }

    Vec4 Vec4::Zero() { return Vec4(impl::glm::vec4(0.f)); }

    Vec4 Vec4::One() { return Vec4(impl::glm::vec4(1.f)); }
} // namespace VoidArchitect::Math
