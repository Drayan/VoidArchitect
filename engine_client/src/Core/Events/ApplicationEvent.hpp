//
// Created by Michael Desmedt on 13/05/2025.
//
#pragma once

#include "Core/Core.hpp"
#include "Event.hpp"

#include <sstream>

namespace VoidArchitect
{
    class VA_API WindowResizedEvent : public Event
    {
    public:
        WindowResizedEvent(unsigned int width, unsigned int height) :
            m_Width(width), m_Height(height)
        {
        }

        inline unsigned int GetWidth() const { return m_Width; }
        inline unsigned int GetHeight() const { return m_Height; }

        std::string ToString() const override
        {
            std::stringstream ss;
            ss << "WindowResizedEvent: " << m_Width << ", " << m_Height;
            return ss.str();
        }

        EVENT_CLASS_CATEGORY(EventCategoryApplication)
        EVENT_CLASS_TYPE(WindowResized)

    private:
        unsigned int m_Width, m_Height;
    };

    class VA_API WindowCloseEvent : public Event
    {
    public:
        WindowCloseEvent() = default;

        EVENT_CLASS_CATEGORY(EventCategoryApplication)
        EVENT_CLASS_TYPE(WindowClose)
    };
} // namespace VoidArchitect
