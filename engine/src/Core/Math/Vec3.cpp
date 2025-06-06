//
// Created by Michael Desmedt on 20/05/2025.
//
#include "Vec3.hpp"

namespace VoidArchitect::Math
{
    Vec3::Vec3()
        : m_Vector()
    {
    }

    Vec3::Vec3(const impl::glm::vec3& vector)
        : m_Vector(vector)
    {
    }

    Vec3::Vec3(const float x, const float y, const float z)
        : m_Vector(x, y, z)
    {
    }

    void Vec3::Normalize() { m_Vector = impl::glm::normalize(m_Vector); }

    bool Vec3::IsZero() const { return m_Vector == impl::glm::vec3(0.f); }

    Vec3 Vec3::operator+(const Vec3& other) const { return Vec3(m_Vector + other.m_Vector); }

    Vec3 Vec3::operator-(const Vec3& other) const { return Vec3(m_Vector - other.m_Vector); }

    Vec3 Vec3::operator*(const Vec3& other) const { return Vec3(m_Vector * other.m_Vector); }

    Vec3 Vec3::operator/(const Vec3& other) const { return Vec3(m_Vector / other.m_Vector); }

    Vec3 Vec3::operator*(const float scalar) const { return Vec3(m_Vector * scalar); }

    Vec3 Vec3::operator/(const float scalar) const { return Vec3(m_Vector / scalar); }

    Vec3& Vec3::operator+=(const Vec3& other)
    {
        m_Vector += other.m_Vector;
        return *this;
    }

    Vec3& Vec3::operator-=(const Vec3& other)
    {
        m_Vector -= other.m_Vector;
        return *this;
    }

    Vec3& Vec3::operator-()
    {
        m_Vector = -m_Vector;
        return *this;
    }

    Vec3 Vec3::Zero() { return Vec3(impl::glm::vec3(0.f)); }

    Vec3 Vec3::One() { return Vec3(impl::glm::vec3(1.f)); }

    Vec3 Vec3::Left() { return Vec3(impl::glm::vec3(-1.f, 0.f, 0.f)); }

    Vec3 Vec3::Right() { return Vec3(impl::glm::vec3(1.f, 0.f, 0.f)); }

    Vec3 Vec3::Up() { return Vec3(impl::glm::vec3(0.f, 1.f, 0.f)); }

    Vec3 Vec3::Down() { return Vec3(impl::glm::vec3(0.f, -1.f, 0.f)); }

    Vec3 Vec3::Forward() { return Vec3(impl::glm::vec3(0.f, 0.f, -1.f)); }

    Vec3 Vec3::Back() { return Vec3(impl::glm::vec3(0.f, 0.f, 1.f)); }

    Vec3 Vec3::Cross(const Vec3& v1, const Vec3& v2)
    {
        return Vec3(impl::glm::cross(v1.m_Vector, v2.m_Vector));
    }
} // namespace VoidArchitect::Math
