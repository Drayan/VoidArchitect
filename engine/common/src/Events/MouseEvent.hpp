//
// Created by Michael Desmedt on 13/05/2025.
//
#pragma once

#include "Core/Core.hpp"
#include "Event.hpp"

namespace VoidArchitect
{
    class VA_API MouseEvent : public Event
    {
    public:
        inline unsigned int GetX() const { return m_MouseX; }
        inline unsigned int GetY() const { return m_MouseY; }

        EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)
    protected:
        MouseEvent(unsigned int x, unsigned int y) : m_MouseX(x), m_MouseY(y) {}
        unsigned int m_MouseX, m_MouseY;
    };

    class VA_API MouseMovedEvent : public MouseEvent
    {
    public:
        MouseMovedEvent(unsigned int x, unsigned int y) : MouseEvent(x, y) {}

        // NOTE As this is purely for debugging purposes, we don't care about performance here.
        std::string ToString() const override
        {
            std::stringstream ss;
            ss << "MouseMovedEvent: " << m_MouseX << ", " << m_MouseY;
            return ss.str();
        }

        EVENT_CLASS_TYPE(MouseMoved)
    };

    class VA_API MouseScrolledEvent : public MouseEvent
    {
    public:
        MouseScrolledEvent(unsigned int x, unsigned int y, float xDelta, float yDelta)
            : MouseEvent(x, y),
              m_XDelta(xDelta),
              m_YDelta(yDelta)
        {
        }

        inline float GetXDelta() const { return m_XDelta; }
        inline float GetYDelta() const { return m_YDelta; }

        // NOTE As this is purely for debugging purposes, we don't care about performance here.
        std::string ToString() const override
        {
            std::stringstream ss;
            ss << "MouseScrolledEvent: " << m_MouseX << ", " << m_MouseY << " (" << m_XDelta << ", "
               << m_YDelta << ")";
            return ss.str();
        }

        EVENT_CLASS_TYPE(MouseScrolled)
    protected:
        float m_XDelta;
        float m_YDelta;
    };

    class VA_API MouseButtonPressedEvent : public MouseEvent
    {
    public:
        MouseButtonPressedEvent(unsigned int x, unsigned int y, unsigned int button)
            : MouseEvent(x, y),
              m_Button(button)
        {
        }

        inline unsigned int GetMouseButton() const { return m_Button; }

        std::string ToString() const override
        {
            std::stringstream ss;
            ss << "MouseButtonPressedEvent: " << m_MouseX << ", " << m_MouseY << " (" << m_Button
               << ")";
            return ss.str();
        }

        EVENT_CLASS_TYPE(MouseButtonPressed)
    protected:
        unsigned int m_Button;
    };

    class VA_API MouseButtonReleasedEvent : public MouseEvent
    {
    public:
        MouseButtonReleasedEvent(unsigned int x, unsigned int y, unsigned int button)
            : MouseEvent(x, y),
              m_Button(button)
        {
        }

        inline unsigned int GetMouseButton() const { return m_Button; }

        std::string ToString() const override
        {
            std::stringstream ss;
            ss << "MouseButtonReleasedEvent: " << m_MouseX << ", " << m_MouseY << " (" << m_Button
               << ")";
            return ss.str();
        }

        EVENT_CLASS_TYPE(MouseButtonReleased)
    protected:
        unsigned int m_Button;
    };
} // namespace VoidArchitect
