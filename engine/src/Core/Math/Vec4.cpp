//
// Created by Michael Desmedt on 22/05/2025.
//
#include "Vec4.hpp"
#include "Vec3.hpp"

#include "Core/Logger.hpp"

namespace VoidArchitect::Math
{
    Vec4::Vec4() : m_Vector() {}

    Vec4::Vec4(const impl::glm::vec4& vector) : m_Vector(vector) {}

    Vec4::Vec4(const float x, const float y, const float z, const float w) : m_Vector(x, y, z, w) {}

    Vec4::Vec4(Math::Vec3 vector, float w) : m_Vector(vector.X(), vector.Y(), vector.Z(), w) {}

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

    Vec4 Vec4::FromString(const std::string& str)
    {
        // Remove whitespace
        std::string cleanStr = str;
        std::erase_if(cleanStr, ::isspace);

        // Check for brackets
        if (cleanStr.empty() || cleanStr.front() != '[' || cleanStr.back() != ']')
        {
            VA_ENGINE_WARN("[Vec4] Invalid string format, expected [x, y, z, w] but got: {}", str);
            return Vec4();
        }

        // Remove brackets
        cleanStr = cleanStr.substr(1, cleanStr.size() - 2);

        // Split by comma
        VAArray<float> values;
        std::stringstream ss(cleanStr);
        std::string token;

        while (std::getline(ss, token, ','))
        {
            try
            {
                float value = std::stof(token);
                values.push_back(value);
            }
            catch (const std::exception& e)
            {
                VA_ENGINE_WARN("[Vec4] Failed to parse token {} as float in '{}'", token, str);
                return Vec4();
            }
        }

        // Check if we have exactly 4 values
        if (values.size() != 4)
        {
            VA_ENGINE_WARN("[Vec4] Invalid string format, expected [x, y, z, w] but got: {}", str);
            return Vec4();
        }

        return Vec4(values[0], values[1], values[2], values[3]);
    }
} // namespace VoidArchitect::Math
