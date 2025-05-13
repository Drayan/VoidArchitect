//
// Created by Michael Desmedt on 13/05/2025.
//
#pragma once

#include "Core/Core.hpp"

namespace VoidArchitect
{
    // Event in the client part of VoidArchitect is currently blocking, meaning when an event occurs, it immediately
    // gets dispatched and must be dealt with. For the future, a better strategy might be to buffer events
    // on an event bus and process them during the "event" phase of the update stage.
    enum class EventType
    {
        None = 0,
        WindowClose,
        WindowResized,
        WindowFocus,
        WindowLostFocus,
        WindowMoved,
        KeyPressed,
        KeyReleased,
        MouseButtonPressed,
        MouseButtonReleased,
        MouseMoved,
        MouseScrolled
    };

    enum EventCategory
    {
        None = 0,
        EventCategoryApplication = BIT(0),
        EventCategoryInput = BIT(1),
        EventCategoryKeyboard = BIT(2),
        EventCategoryMouse = BIT(3),
        EventCategoryMouseButton = BIT(4)
    };

#define EVENT_CLASS_TYPE(type)                                                                                         \
    static EventType GetStaticType() { return EventType::type; }                                                       \
    virtual EventType GetEventType() const override { return GetStaticType(); }                                        \
    virtual const char* GetName() const override { return #type; }

#define EVENT_CLASS_CATEGORY(category)                                                                                 \
    virtual int GetCategoryFlags() const override { return category; }

    class VA_API Event
    {
        friend class EventDispatcher;

    public:
        inline virtual EventType GetEventType() const = 0;
        inline virtual const char* GetName() const = 0;
        inline virtual int GetCategoryFlags() const = 0;
        inline virtual std::string ToString() const { return GetName(); }

        inline bool IsInCategory(EventCategory category) { return GetCategoryFlags() & category; }

    protected:
        bool m_Handled = false;
    };

    class EventDispatcher
    {
        template <typename T>
        using EventFn = std::function<bool(T&)>;

    public:
        EventDispatcher(Event& event) : m_Event(event) {}

        template <typename T>
        bool Dispatch(EventFn<T> func)
        {
            if (m_Event.GetEventType() == T::GetStaticType())
            {
                m_Event.m_Handled = func(*static_cast<T*>(&m_Event));
                return true;
            }

            return false;
        }

    private:
        Event& m_Event;
    };
} // namespace VoidArchitect
