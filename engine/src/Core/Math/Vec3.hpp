//
// Created by Michael Desmedt on 20/05/2025.
//
#pragma once

namespace VoidArchitect::Math
{
    namespace impl
    {
#include <glm/vec3.hpp>
    }

    class Vec3
    {
        friend class Mat4;

    public:
        static Vec3 Zero();
        static Vec3 One();
        static Vec3 Left();
        static Vec3 Right();
        static Vec3 Up();
        static Vec3 Down();
        static Vec3 Forward();
        static Vec3 Back();

        Vec3();
        Vec3(float x, float y, float z);

    private:
        explicit Vec3(const impl::glm::vec3& vector);

        impl::glm::vec3 m_Vector;
    };
}
