//
// Created by Michael Desmedt on 31/05/2025.
//
#include "Vec2.hpp"

namespace VoidArchitect
{
    namespace Math
    {
        Vec2::Vec2()
            : m_Vector()
        {
        }

        Vec2::Vec2(const impl::glm::vec2& vector)
            : m_Vector(vector)
        {
        }

        Vec2::Vec2(const float x, const float y)
            : m_Vector(x, y)
        {
        }

        Vec2 Vec2::operator+(const Vec2& other) const
        {
            return Vec2(m_Vector + other.m_Vector);
        }

        Vec2 Vec2::operator-(const Vec2& other) const
        {
            return Vec2(m_Vector - other.m_Vector);
        }

        Vec2 Vec2::operator*(const Vec2& other) const
        {
            return Vec2(m_Vector * other.m_Vector);
        }

        Vec2 Vec2::operator/(const Vec2& other) const
        {
            return Vec2(m_Vector / other.m_Vector);
        }

        Vec2 Vec2::operator*(const float scalar) const
        {
            return Vec2(m_Vector * scalar);
        }

        Vec2 Vec2::operator/(const float scalar) const
        {
            return Vec2(m_Vector / scalar);
        }

        Vec2& Vec2::operator+=(const Vec2& other)
        {
            m_Vector += other.m_Vector;
            return *this;
        }

        Vec2& Vec2::operator-=(const Vec2& other)
        {
            m_Vector -= other.m_Vector;
            return *this;
        }

        Vec2 Vec2::Zero()
        {
            return Vec2(impl::glm::vec2(0.f));
        }

        Vec2 Vec2::One()
        {
            return Vec2(impl::glm::vec2(1.f));
        }

        Vec2 Vec2::Left()
        {
            return Vec2(impl::glm::vec2(-1.f, 0.f));
        }

        Vec2 Vec2::Right()
        {
            return Vec2(impl::glm::vec2(1.f, 0.f));
        }

        Vec2 Vec2::Up()
        {
            return Vec2(impl::glm::vec2(0.f, 1.f));
        }

        Vec2 Vec2::Down()
        {
            return Vec2(impl::glm::vec2(0.f, -1.f));
        }
    } // Math
} // VoidArchitect
