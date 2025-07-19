//
// Created by Michael Desmedt on 17/07/2025.
//
#pragma once

#include "Event.hpp"
#include "EventTypes.hpp"

/// @file InputEvents.hpp
/// @brief Input event classes for keyboard and mouse interactions
///
/// This file defines all input-related events in the VoidArchitect engine,
/// including keyboard and mouse events. All input events are configured
/// for deferred processing on the main thread to ensure UI responsiveness.

namespace VoidArchitect::Events
{
    // Forward declarations
    class KeyPressedEvent;
    class KeyReleasedEvent;
    class MouseMovedEvent;
    class MouseButtonPressedEvent;
    class MouseButtonReleasedEvent;
    class MouseScrolledEvent;

    // === Event Traits Specializations ===
    
    /// @brief Keyboard events configuration - deferred processing with normal priority
    ///
    /// All keyboard events are processed on the main thread with deferred execution
    /// to ensure proper UI and input handling synchronization.
    
    /// @brief Event traits for KeyPressedEvent
    EVENT_TRAITS(
        KeyPressedEvent,
        EventExecutionMode::Deferred,  ///< Main thread processing
        EventPriority::Normal,          ///< Normal priority
        true,                          ///< Pooled for performance
        EventCategory::Input | EventCategory::Keyboard); ///< Input + Keyboard categories

    /// @brief Event traits for KeyReleasedEvent
    EVENT_TRAITS(
        KeyReleasedEvent,
        EventExecutionMode::Deferred,  ///< Main thread processing
        EventPriority::Normal,          ///< Normal priority
        true,                          ///< Pooled for performance
        EventCategory::Input | EventCategory::Keyboard); ///< Input + Keyboard categories

    /// @brief Mouse events configuration - deferred processing with normal priority
    ///
    /// All mouse events are processed on the main thread with deferred execution
    /// to ensure proper UI interaction and cursor handling.
    
    /// @brief Event traits for MouseMovedEvent
    EVENT_TRAITS(
        MouseMovedEvent,
        EventExecutionMode::Deferred,  ///< Main thread processing
        EventPriority::Normal,          ///< Normal priority
        true,                          ///< Pooled for performance
        EventCategory::Input | EventCategory::Mouse); ///< Input + Mouse categories

    /// @brief Event traits for MouseButtonPressedEvent
    EVENT_TRAITS(
        MouseButtonPressedEvent,
        EventExecutionMode::Deferred,  ///< Main thread processing
        EventPriority::Normal,          ///< Normal priority
        true,                          ///< Pooled for performance
        EventCategory::Input | EventCategory::Mouse); ///< Input + Mouse categories

    /// @brief Event traits for MouseButtonReleasedEvent
    EVENT_TRAITS(
        MouseButtonReleasedEvent,
        EventExecutionMode::Deferred,  ///< Main thread processing
        EventPriority::Normal,          ///< Normal priority
        true,                          ///< Pooled for performance
        EventCategory::Input | EventCategory::Mouse); ///< Input + Mouse categories

    /// @brief Event traits for MouseScrolledEvent
    EVENT_TRAITS(
        MouseScrolledEvent,
        EventExecutionMode::Deferred,  ///< Main thread processing
        EventPriority::Normal,          ///< Normal priority
        true,                          ///< Pooled for performance
        EventCategory::Input | EventCategory::Mouse); ///< Input + Mouse categories

    // === Keyboard Events ===

    /// @brief Event triggered when a keyboard key is pressed
    ///
    /// @warning Thread-safe emission - handlers executed on main thread via deferred processing
    ///
    /// Represents a keyboard key press event, including support for key repeat.
    /// Key codes follow platform-specific conventions (typically SDL or similar).
    ///
    /// **Usage example:**
    /// @code
    /// // Subscribe to key press events
    /// auto subscription = g_EventSystem->Subscribe<KeyPressedEvent>(
    ///     [](const KeyPressedEvent& e) {
    ///         if (e.GetKeyCode() == SPACE_KEY && !e.IsRepeat()) {
    ///             // Handle initial space press
    ///         }
    ///     });
    ///
    /// // Emit key press event
    /// EMIT_EVENT(KeyPressedEvent, SPACE_KEY, 0);
    /// @endcode
    class VA_API KeyPressedEvent final : public Event
    {
    public:
        /// @brief Construct a key pressed event
        /// @param keycode Platform-specific key code
        /// @param repeatCount Number of automatic repeats (0 for initial press)
        KeyPressedEvent(const int keycode, const int repeatCount)
            : Event(
                  EventTraits<KeyPressedEvent>::Priority,
                  EventTraits<KeyPressedEvent>::ExecutionMode),
              m_KeyCode(keycode),
              m_RepeatCount(repeatCount)
        {
        }

        /// @brief Get the pressed key code
        /// @return Platform-specific key identifier
        int GetKeyCode() const { return m_KeyCode; }
        
        /// @brief Get the repeat count
        /// @return Number of automatic repeats (0 for initial press)
        int GetRepeatCount() const { return m_RepeatCount; }

        /// @brief Check if this is a key repeat event
        /// @return true if this is an automatic repeat, false for initial press
        bool IsRepeat() const { return m_RepeatCount > 0; }

        /// @brief Generate string representation for debugging
        /// @return Human-readable event description
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
        int m_KeyCode;     ///< Platform-specific key identifier
        int m_RepeatCount; ///< Number of automatic repeats (0 = initial press)
    };

    /// @brief Event triggered when a keyboard key is released
    ///
    /// @warning Thread-safe emission - handlers executed on main thread via deferred processing
    ///
    /// Represents a keyboard key release event. Always follows a corresponding
    /// KeyPressedEvent for the same key code.
    ///
    /// **Usage example:**
    /// @code
    /// // Subscribe to key release events
    /// auto subscription = g_EventSystem->Subscribe<KeyReleasedEvent>(
    ///     [](const KeyReleasedEvent& e) {
    ///         if (e.GetKeyCode() == SPACE_KEY) {
    ///             // Handle space key release
    ///         }
    ///     });
    ///
    /// // Emit key release event
    /// EMIT_EVENT(KeyReleasedEvent, SPACE_KEY);
    /// @endcode
    class VA_API KeyReleasedEvent final : public Event
    {
    public:
        /// @brief Construct a key released event
        /// @param keycode Platform-specific key code
        explicit KeyReleasedEvent(const int keycode)
            : Event(
                  EventTraits<KeyReleasedEvent>::Priority,
                  EventTraits<KeyReleasedEvent>::ExecutionMode),
              m_KeyCode(keycode)
        {
        }

        /// @brief Get the released key code
        /// @return Platform-specific key identifier
        int GetKeyCode() const { return m_KeyCode; }

        /// @brief Generate string representation for debugging
        /// @return Human-readable event description
        std::string ToString() const override
        {
            return "KeyReleasedEvent: " + std::to_string(m_KeyCode);
        }

        EVENT_CLASS_TYPE(
            KeyReleasedEvent,
            "KeyReleased",
            EventCategory::Input | EventCategory::Keyboard);

    private:
        int m_KeyCode; ///< Platform-specific key identifier
    };

    // === Mouse Events ===

    /// @brief Event triggered when the mouse cursor moves
    ///
    /// @warning Thread-safe emission - handlers executed on main thread via deferred processing
    ///
    /// Represents mouse cursor movement with absolute screen coordinates.
    /// Generated frequently during mouse movement, so handlers should be efficient.
    ///
    /// **Performance note:**
    /// Mouse move events can be generated at high frequency during mouse movement.
    /// Consider throttling or using delta calculations in handlers for performance.
    ///
    /// **Usage example:**
    /// @code
    /// // Subscribe to mouse movement
    /// auto subscription = g_EventSystem->Subscribe<MouseMovedEvent>(
    ///     [](const MouseMovedEvent& e) {
    ///         // Update cursor position in UI
    ///         UpdateCursor(e.GetX(), e.GetY());
    ///     });
    ///
    /// // Emit mouse movement
    /// EMIT_EVENT(MouseMovedEvent, 640, 480);
    /// @endcode
    class VA_API MouseMovedEvent final : public Event
    {
    public:
        /// @brief Construct a mouse moved event
        /// @param x Absolute X coordinate in pixels
        /// @param y Absolute Y coordinate in pixels
        MouseMovedEvent(const unsigned int x, const unsigned int y)
            : Event(
                  EventTraits<MouseMovedEvent>::Priority,
                  EventTraits<MouseMovedEvent>::ExecutionMode),
              m_MouseX(x),
              m_MouseY(y)
        {
        }

        /// @brief Get the mouse X coordinate
        /// @return Absolute X position in pixels
        uint32_t GetX() const { return m_MouseX; }
        
        /// @brief Get the mouse Y coordinate
        /// @return Absolute Y position in pixels
        uint32_t GetY() const { return m_MouseY; }

        /// @brief Generate string representation for debugging
        /// @return Human-readable event description
        std::string ToString() const override
        {
            return "MouseMovedEvent: " + std::to_string(m_MouseX) + ", " + std::to_string(m_MouseY);
        }

        EVENT_CLASS_TYPE(
            MouseMovedEvent,
            "MouseMoved",
            EventCategory::Input | EventCategory::Mouse);

    private:
        uint32_t m_MouseX; ///< Absolute X coordinate in pixels
        uint32_t m_MouseY; ///< Absolute Y coordinate in pixels
    };

    /// @brief Event triggered when a mouse button is pressed
    ///
    /// @warning Thread-safe emission - handlers executed on main thread via deferred processing
    ///
    /// Represents a mouse button press at a specific screen location.
    /// Includes the cursor position at the time of the press for context.
    ///
    /// **Button codes:**
    /// Button codes are platform-specific but typically follow:
    /// - 0: Left button
    /// - 1: Middle button
    /// - 2: Right button
    /// - 3+: Additional buttons
    ///
    /// **Usage example:**
    /// @code
    /// // Subscribe to mouse button events
    /// auto subscription = g_EventSystem->Subscribe<MouseButtonPressedEvent>(
    ///     [](const MouseButtonPressedEvent& e) {
    ///         if (e.GetButton() == 0) { // Left click
    ///             HandleClick(e.GetX(), e.GetY());
    ///         }
    ///     });
    ///
    /// // Emit mouse button press
    /// EMIT_EVENT(MouseButtonPressedEvent, 640, 480, 0); // Left click at (640,480)
    /// @endcode
    class VA_API MouseButtonPressedEvent final : public Event
    {
    public:
        /// @brief Construct a mouse button pressed event
        /// @param x Absolute X coordinate where button was pressed
        /// @param y Absolute Y coordinate where button was pressed
        /// @param button Platform-specific button identifier
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

        /// @brief Get the X coordinate of the button press
        /// @return Absolute X position in pixels
        uint32_t GetX() const { return m_MouseX; }
        
        /// @brief Get the Y coordinate of the button press
        /// @return Absolute Y position in pixels
        uint32_t GetY() const { return m_MouseY; }
        
        /// @brief Get the pressed button identifier
        /// @return Platform-specific button code
        uint32_t GetButton() const { return m_Button; }

        /// @brief Generate string representation for debugging
        /// @return Human-readable event description
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
        uint32_t m_MouseX; ///< Absolute X coordinate where button was pressed
        uint32_t m_MouseY; ///< Absolute Y coordinate where button was pressed
        uint32_t m_Button; ///< Platform-specific button identifier
    };

    /// @brief Event triggered when a mouse button is released
    ///
    /// @warning Thread-safe emission - handlers executed on main thread via deferred processing
    ///
    /// Represents a mouse button release at a specific screen location.
    /// Always follows a corresponding MouseButtonPressedEvent for the same button.
    ///
    /// **Button codes:**
    /// Button codes are platform-specific but typically follow:
    /// - 0: Left button
    /// - 1: Middle button
    /// - 2: Right button
    /// - 3+: Additional buttons
    ///
    /// **Usage example:**
    /// @code
    /// // Subscribe to mouse button release events
    /// auto subscription = g_EventSystem->Subscribe<MouseButtonReleasedEvent>(
    ///     [](const MouseButtonReleasedEvent& e) {
    ///         if (e.GetButton() == 0) { // Left button release
    ///             HandleClickRelease(e.GetX(), e.GetY());
    ///         }
    ///     });
    ///
    /// // Emit mouse button release
    /// EMIT_EVENT(MouseButtonReleasedEvent, 640, 480, 0); // Left release at (640,480)
    /// @endcode
    class VA_API MouseButtonReleasedEvent final : public Event
    {
    public:
        /// @brief Construct a mouse button released event
        /// @param x Absolute X coordinate where button was released
        /// @param y Absolute Y coordinate where button was released
        /// @param button Platform-specific button identifier
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

        /// @brief Get the X coordinate of the button release
        /// @return Absolute X position in pixels
        uint32_t GetX() const { return m_MouseX; }
        
        /// @brief Get the Y coordinate of the button release
        /// @return Absolute Y position in pixels
        uint32_t GetY() const { return m_MouseY; }
        
        /// @brief Get the released button identifier
        /// @return Platform-specific button code
        uint32_t GetButton() const { return m_Button; }

        /// @brief Generate string representation for debugging
        /// @return Human-readable event description
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
        uint32_t m_MouseX; ///< Absolute X coordinate where button was released
        uint32_t m_MouseY; ///< Absolute Y coordinate where button was released
        uint32_t m_Button; ///< Platform-specific button identifier
    };

    /// @brief Event triggered when the mouse wheel is scrolled
    ///
    /// @warning Thread-safe emission - handlers executed on main thread via deferred processing
    ///
    /// Represents mouse wheel scrolling with both horizontal and vertical components.
    /// Includes the cursor position at the time of scrolling for context.
    ///
    /// **Delta interpretation:**
    /// - Positive Y delta: Scroll up/away from user
    /// - Negative Y delta: Scroll down/toward user
    /// - Positive X delta: Scroll right
    /// - Negative X delta: Scroll left
    /// - Delta magnitude: Platform-specific scroll amount
    ///
    /// **Usage example:**
    /// @code
    /// // Subscribe to mouse scroll events
    /// auto subscription = g_EventSystem->Subscribe<MouseScrolledEvent>(
    ///     [](const MouseScrolledEvent& e) {
    ///         float zoom = e.GetYDelta() * 0.1f; // Convert to zoom factor
    ///         ZoomAtPosition(e.GetX(), e.GetY(), zoom);
    ///     });
    ///
    /// // Emit mouse scroll event
    /// EMIT_EVENT(MouseScrolledEvent, 640, 480, 0.0f, 1.0f); // Vertical scroll up
    /// @endcode
    class VA_API MouseScrolledEvent final : public Event
    {
    public:
        /// @brief Construct a mouse scrolled event
        /// @param x Absolute X coordinate where scrolling occurred
        /// @param y Absolute Y coordinate where scrolling occurred
        /// @param xDelta Horizontal scroll delta (positive = right)
        /// @param yDelta Vertical scroll delta (positive = up)
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

        /// @brief Get the X coordinate where scrolling occurred
        /// @return Absolute X position in pixels
        uint32_t GetX() const { return m_MouseX; }
        
        /// @brief Get the Y coordinate where scrolling occurred
        /// @return Absolute Y position in pixels
        uint32_t GetY() const { return m_MouseY; }
        
        /// @brief Get the horizontal scroll delta
        /// @return Horizontal scroll amount (positive = right)
        float GetXDelta() const { return m_XDelta; }
        
        /// @brief Get the vertical scroll delta
        /// @return Vertical scroll amount (positive = up)
        float GetYDelta() const { return m_YDelta; }

        /// @brief Generate string representation for debugging
        /// @return Human-readable event description
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
        uint32_t m_MouseX; ///< Absolute X coordinate where scrolling occurred
        uint32_t m_MouseY; ///< Absolute Y coordinate where scrolling occurred
        float m_XDelta;    ///< Horizontal scroll delta (positive = right)
        float m_YDelta;    ///< Vertical scroll delta (positive = up)
    };
}
