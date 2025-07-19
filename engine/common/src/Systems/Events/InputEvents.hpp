//
// Created by Michael Desmedt on 17/07/2025.
//
#pragma once

#include "Event.hpp"
#include "EventTypes.hpp"

namespace VoidArchitect::Events
{
    class KeyPressedEvent;
    class KeyReleasedEvent;
    class MouseMovedEvent;
    class MouseButtonPressedEvent;
    class MouseButtonReleasedEvent;
    class MouseScrolledEvent;

    // === Event Traits Specializations ===

    // Keyboard events - deferred processing with normal priority
    EVENT_TRAITS(
        KeyPressedEvent,
        EventExecutionMode::Deferred,
        EventPriority::Normal,
        true,
        EventCategory::Input | EventCategory::Keyboard);

    EVENT_TRAITS(
        KeyReleasedEvent,
        EventExecutionMode::Deferred,
        EventPriority::Normal,
        true,
        EventCategory::Input | EventCategory::Keyboard);

    // Mouse events - deferred processing with normal priority
    EVENT_TRAITS(
        MouseMovedEvent,
        EventExecutionMode::Deferred,
        EventPriority::Normal,
        true,
        EventCategory::Input | EventCategory::Mouse);

    EVENT_TRAITS(
        MouseButtonPressedEvent,
        EventExecutionMode::Deferred,
        EventPriority::Normal,
        true,
        EventCategory::Input | EventCategory::Mouse);

    EVENT_TRAITS(
        MouseButtonReleasedEvent,
        EventExecutionMode::Deferred,
        EventPriority::Normal,
        true,
        EventCategory::Input | EventCategory::Mouse);

    EVENT_TRAITS(
        MouseScrolledEvent,
        EventExecutionMode::Deferred,
        EventPriority::Normal,
        true,
        EventCategory::Input | EventCategory::Mouse);

    // === Keyboard Events ===

    class VA_API KeyPressedEvent final : public Event
    {
    public:
        KeyPressedEvent(const int keycode, const int repeatCount)
            : Event(
                  EventTraits<KeyPressedEvent>::Priority,
                  EventTraits<KeyPressedEvent>::ExecutionMode),
              m_KeyCode(keycode),
              m_RepeatCount(repeatCount)
        {
        }

        int GetKeyCode() const { return m_KeyCode; }
        int GetRepeatCount() const { return m_RepeatCount; }

        bool IsRepeat() const { return m_RepeatCount > 0; }

        std::string ToString() const override
        {
            return "KeyPressedEvent: " + std::to_string(m_KeyCode) + " (" + std::to_string(
                m_RepeatCount) + " repeats)";
        }

        EVENT_CLASS_TYPE(
            KeyPressedEvent,
            "KeyPressed",
            EventCategory::Input | EventCategory::Keyboard);

    private:
        int m_KeyCode;
        int m_RepeatCount;
    };

    class VA_API KeyReleasedEvent final : public Event
    {
    public:
        explicit KeyReleasedEvent(const int keycode)
            : Event(
                  EventTraits<KeyReleasedEvent>::Priority,
                  EventTraits<KeyReleasedEvent>::ExecutionMode),
              m_KeyCode(keycode)
        {
        }

        int GetKeyCode() const { return m_KeyCode; }

        std::string ToString() const override
        {
            return "KeyReleasedEvent: " + std::to_string(m_KeyCode);
        }

        EVENT_CLASS_TYPE(
            KeyReleasedEvent,
            "KeyReleased",
            EventCategory::Input | EventCategory::Keyboard);

    private:
        int m_KeyCode;
    };

    // === Mouse Events ===

    class VA_API MouseMovedEvent final : public Event
    {
    public:
        MouseMovedEvent(const unsigned int x, const unsigned int y)
            : Event(
                  EventTraits<MouseMovedEvent>::Priority,
                  EventTraits<MouseMovedEvent>::ExecutionMode),
              m_MouseX(x),
              m_MouseY(y)
        {
        }

        uint32_t GetX() const { return m_MouseX; }
        uint32_t GetY() const { return m_MouseY; }

        std::string ToString() const override
        {
            return "MouseMovedEvent: " + std::to_string(m_MouseX) + ", " + std::to_string(m_MouseY);
        }

        EVENT_CLASS_TYPE(
            MouseMovedEvent,
            "MouseMoved",
            EventCategory::Input | EventCategory::Mouse);

    private:
        uint32_t m_MouseX;
        uint32_t m_MouseY;
    };

    class VA_API MouseButtonPressedEvent final : public Event
    {
    public:
        MouseButtonPressedEvent(
            const unsigned int x,
            const unsigned int y,
            const unsigned int button)
            : Event(
                  EventTraits<MouseButtonPressedEvent>::Priority,
                  EventTraits<MouseButtonPressedEvent>::ExecutionMode),
              m_MouseX(x),
              m_MouseY(y),
              m_Button(button)
        {
        }

        uint32_t GetX() const { return m_MouseX; }
        uint32_t GetY() const { return m_MouseY; }
        uint32_t GetButton() const { return m_Button; }

        std::string ToString() const override
        {
            return "MouseButtonPressedEvent: " + std::to_string(m_MouseX) + ", " + std::to_string(
                m_MouseY) + ", " + std::to_string(m_Button);
        }

        EVENT_CLASS_TYPE(
            MouseButtonPressedEvent,
            "MouseButtonPressed",
            EventCategory::Input | EventCategory::Mouse);

    private:
        uint32_t m_MouseX;
        uint32_t m_MouseY;
        uint32_t m_Button;
    };

    class VA_API MouseButtonReleasedEvent final : public Event
    {
    public:
        MouseButtonReleasedEvent(
            const unsigned int x,
            const unsigned int y,
            const unsigned int button)
            : Event(
                  EventTraits<MouseButtonReleasedEvent>::Priority,
                  EventTraits<MouseButtonReleasedEvent>::ExecutionMode),
              m_MouseX(x),
              m_MouseY(y),
              m_Button(button)
        {
        }

        uint32_t GetX() const { return m_MouseX; }
        uint32_t GetY() const { return m_MouseY; }
        uint32_t GetButton() const { return m_Button; }

        std::string ToString() const override
        {
            return "MouseButtonReleasedEvent: " + std::to_string(m_MouseX) + ", " + std::to_string(
                m_MouseY) + ", " + std::to_string(m_Button);
        }

        EVENT_CLASS_TYPE(
            MouseButtonReleasedEvent,
            "MouseButtonReleased",
            EventCategory::Input | EventCategory::Mouse);

    private:
        uint32_t m_MouseX;
        uint32_t m_MouseY;
        uint32_t m_Button;
    };

    class VA_API MouseScrolledEvent final : public Event
    {
    public:
        MouseScrolledEvent(
            const uint32_t x,
            const uint32_t y,
            const float xDelta,
            const float yDelta)
            : Event(
                  EventTraits<MouseScrolledEvent>::Priority,
                  EventTraits<MouseScrolledEvent>::ExecutionMode),
              m_MouseX(x),
              m_MouseY(y),
              m_XDelta(xDelta),
              m_YDelta(yDelta)
        {
        }

        uint32_t GetX() const { return m_MouseX; }
        uint32_t GetY() const { return m_MouseY; }
        float GetXDelta() const { return m_XDelta; }
        float GetYDelta() const { return m_YDelta; }

        std::string ToString() const override
        {
            return "MouseScrolledEvent: " + std::to_string(m_MouseX) + ", " +
                std::to_string(m_MouseY) + ", " + std::to_string(m_XDelta) + ", " + std::to_string(
                    m_YDelta);
        }

        EVENT_CLASS_TYPE(
            MouseScrolledEvent,
            "MouseScrolled",
            EventCategory::Input | EventCategory::Mouse);

    private:
        uint32_t m_MouseX;
        uint32_t m_MouseY;
        float m_XDelta;
        float m_YDelta;
    };
}
