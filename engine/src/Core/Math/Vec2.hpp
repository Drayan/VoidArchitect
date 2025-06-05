//
// Created by Michael Desmedt on 31/05/2025.
//
#pragma once

namespace VoidArchitect
{
    namespace Math
    {
        namespace impl
        {
#include <glm/glm.hpp>
        }

        class Vec2
        {
        public:
            static Vec2 Zero();
            static Vec2 One();
            static Vec2 Left();
            static Vec2 Right();
            static Vec2 Up();
            static Vec2 Down();

            Vec2();
            Vec2(float x, float y);

            Vec2 operator+(const Vec2& other) const;
            Vec2 operator-(const Vec2& other) const;
            Vec2 operator*(const Vec2& other) const;
            Vec2 operator/(const Vec2& other) const;
            Vec2 operator*(const float scalar) const;
            Vec2 operator/(const float scalar) const;
            Vec2& operator+=(const Vec2& other);
            Vec2& operator-=(const Vec2& other);

            void X(const float x) { m_Vector.x = x; }
            void Y(const float y) { m_Vector.y = y; }

            float X() const { return m_Vector.x; }
            float Y() const { return m_Vector.y; }

        private:
            explicit Vec2(const impl::glm::vec2& vector);

            impl::glm::vec2 m_Vector;
        };
    } // namespace Math
} // namespace VoidArchitect
