//
// Created by Michael Desmedt on 17/07/2025.
//
#pragma once

#include "Event.hpp"
#include "EventTypes.hpp"

/// @file WindowEvents.hpp
/// @brief Window-related event classes for application lifecycle management
///
/// This file defines all window-related events in the VoidArchitect engine,
/// including window close, resize, focus, and movement events. These events
/// handle critical application lifecycle management and UI state synchronisation.
///
/// **Event processing modes:**
/// - WindowCloseEvent: Immediate processing (critical for clean shutdown)
/// - WindowResizedEvent: Immediate processing (critical for rendering surface updates)
/// - WindowFocusEvent/WindowLostFocusEvent: Deferred processing (normal priority)
/// - WindowMovedEvent: Deferred processing (low priority)

namespace VoidArchitect::Events
{
    // Forward declarations
    class WindowCloseEvent;
    class WindowResizedEvent;
    class WindowFocusEvent;
    class WindowLostFocusEvent;
    class WindowMovedEvent;

    // === Event Traits Specializations ===
    
    /// @brief Critical window events require immediate processing
    ///
    /// Window close and resize events are processed immediately to ensure
    /// responsive application termination and rendering surface updates.

    /// @brief Event traits for WindowCloseEvent - immediate critical processing
    EVENT_TRAITS(
        WindowCloseEvent,
        EventExecutionMode::Immediate,  ///< Immediate processing for responsive shutdown
        Jobs::JobPriority::Critical,    ///< Critical priority for clean termination
        true,                          ///< Pooled for performance
        EventCategory::Application);   ///< Application category

    /// @brief Event traits for WindowResizedEvent - immediate critical processing
    EVENT_TRAITS(
        WindowResizedEvent,
        EventExecutionMode::Immediate,  ///< Immediate processing for rendering updates
        EventPriority::Critical,        ///< Critical priority for UI responsiveness
        true,                          ///< Pooled for performance
        EventCategory::Application);   ///< Application category

    /// @brief Standard window events use deferred processing
    ///
    /// Focus and movement events are processed on the main thread with
    /// deferred execution as they are not time-critical.

    /// @brief Event traits for WindowFocusEvent - deferred normal processing
    EVENT_TRAITS(
        WindowFocusEvent,
        EventExecutionMode::Deferred,   ///< Deferred main thread processing
        EventPriority::Normal,          ///< Normal priority
        true,                          ///< Pooled for performance
        EventCategory::Application);   ///< Application category

    /// @brief Event traits for WindowLostFocusEvent - deferred normal processing
    EVENT_TRAITS(
        WindowLostFocusEvent,
        EventExecutionMode::Deferred,   ///< Deferred main thread processing
        EventPriority::Normal,          ///< Normal priority
        true,                          ///< Pooled for performance
        EventCategory::Application);   ///< Application category

    /// @brief Event traits for WindowMovedEvent - deferred low priority processing
    EVENT_TRAITS(
        WindowMovedEvent,
        EventExecutionMode::Deferred,   ///< Deferred main thread processing
        EventPriority::Low,             ///< Low priority (position changes not critical)
        true,                          ///< Pooled for performance
        EventCategory::Application);   ///< Application category

    // === Events Declarations ===

    /// @brief Event emitted when the main window is requested to close
    ///
    /// @warning Thread-safe emission - handlers executed immediately in emitting thread
    ///
    /// WindowCloseEvent is triggered when the user requests to close the application
    /// window through system means (clicking X, Alt+F4, etc.). This event requires
    /// immediate processing to ensure responsive application termination.
    ///
    /// **Processing characteristics:**
    /// - Execution mode: Immediate (cannot be deferred)
    /// - Priority: Critical (highest priority)
    /// - Main thread: Required (application termination)
    /// - Typical handlers: Application::OnWindowClose, clean-up routines
    ///
    /// **Handler responsibilities:**
    /// - Save user data and application state
    /// - Perform graceful clean-up of resources
    /// - Set application running flag to false
    /// - Handle "save before exit" dialogues if needed
    ///
    /// **Usage example:**
    /// @code
    /// auto subscription = g_EventSystem->Subscribe<WindowCloseEvent>(
    ///     [this](const WindowCloseEvent& e) {
    ///         SaveUserData();
    ///         m_Running = false;
    ///     });
    /// @endcode
    class VA_API WindowCloseEvent final : public Event
    {
    public:
        /// @brief Construct a window close event
        WindowCloseEvent()
            : Event(
                EventTraits<WindowCloseEvent>::Priority,
                EventTraits<WindowCloseEvent>::ExecutionMode)
        {
        }

        /// @brief Get debug string representation
        /// @return Human-readable description of the event
        std::string ToString() const override
        {
            return "WindowCloseEvent";
        }

        EVENT_CLASS_TYPE(WindowCloseEvent, "WindowClose", EventCategory::Application);
    };

    /// @brief Event emitted when the main window is resized
    ///
    /// @warning Thread-safe emission - handlers executed immediately in emitting thread
    ///
    /// WindowResizedEvent is triggered when the application window size changes
    /// due to user interaction, system events, or programmatic changes. This event
    /// requires immediate processing for rendering surface updates.
    ///
    /// **Processing characteristics:**
    /// - Execution mode: Immediate (critical for rendering surface updates)
    /// - Priority: Critical (responsive UI updates)
    /// - Main thread: Required (rendering surface management)
    /// - Typical handlers: Renderer updates, UI layout updates, camera aspect ratio
    ///
    /// **Handler responsibilities:**
    /// - Update rendering surface dimensions
    /// - Recreate swap chains and render targets
    /// - Update camera aspect ratios
    /// - Trigger UI layout recalculations
    /// - Update viewport and scissor rectangles
    ///
    /// **Usage example:**
    /// @code
    /// auto subscription = g_EventSystem->Subscribe<WindowResizedEvent>(
    ///     [this](const WindowResizedEvent& e) {
    ///         m_Renderer->UpdateSurfaceSize(e.GetWidth(), e.GetHeight());
    ///         m_Camera->SetAspectRatio(e.GetAspectRatio());
    ///     });
    /// @endcode
    class VA_API WindowResizedEvent final : public Event
    {
    public:
        /// @brief Construct window resize event with new dimensions
        /// @param width New window width in pixels
        /// @param height New window height in pixels
        WindowResizedEvent(const uint32_t width, const uint32_t height)
            : Event(
                  EventTraits<WindowResizedEvent>::Priority,
                  EventTraits<WindowResizedEvent>::ExecutionMode),
              m_Width(width),
              m_Height(height)
        {
        }

        /// @brief Get new window width
        /// @return Window width in pixels
        uint32_t GetWidth() const { return m_Width; }

        /// @brief Get new window height
        /// @return Window height in pixels
        uint32_t GetHeight() const { return m_Height; }

        /// @brief Get window aspect ratio
        /// @return Width/height ratio as float
        float GetAspectRatio() const
        {
            return m_Height > 0 ? static_cast<float>(m_Width) / static_cast<float>(m_Height) : 1.0f;
        }

        /// @brief Check if the window size is valid
        /// @return true if both width and height are greater than zero
        bool IsValidSize() const
        {
            return m_Width > 0 && m_Height > 0;
        }

        /// @brief Get debug string representation
        /// @return Human-readable description of the event with dimensions
        std::string ToString() const override
        {
            return "WindowResizedEvent: " + std::to_string(m_Width) + " x " + std::to_string(
                m_Height);
        }

        EVENT_CLASS_TYPE(WindowResizedEvent, "WindowResize", EventCategory::Application);

    private:
        uint32_t m_Width;  ///< New window width in pixels
        uint32_t m_Height; ///< New window height in pixels
    };

    /// @brief Event emitted when the main window gains focus
    ///
    /// @warning Thread-safe emission - handlers executed on main thread via deferred processing
    ///
    /// WindowFocusEvent is triggered when the application window becomes the
    /// active/focused window in the operating system. This is useful for
    /// resuming game logic, audio, or other activities when the application
    /// becomes active again.
    ///
    /// **Processing characteristics:**
    /// - Execution mode: Deferred (main thread for UI updates)
    /// - Priority: Normal (not critical but timely)
    /// - Main thread: Required (UI state management)
    /// - Typical handlers: Resume game logic, enable audio, show cursor
    ///
    /// **Handler responsibilities:**
    /// - Resume paused game logic and timers
    /// - Restore audio volume and effects
    /// - Re-enable input processing
    /// - Update UI to active state
    ///
    /// **Usage example:**
    /// @code
    /// auto subscription = g_EventSystem->Subscribe<WindowFocusEvent>(
    ///     [this](const WindowFocusEvent& e) {
    ///         m_AudioSystem->SetMasterVolume(m_SavedVolume);
    ///         m_GameLogic->Resume();
    ///     });
    /// @endcode
    class VA_API WindowFocusEvent final : public Event
    {
    public:
        /// @brief Construct a window focus event
        WindowFocusEvent()
            : Event(
                EventTraits<WindowFocusEvent>::Priority,
                EventTraits<WindowFocusEvent>::ExecutionMode)
        {
        }

        /// @brief Get debug string representation
        /// @return Human-readable description of the event
        std::string ToString() const override
        {
            return "WindowFocusEvent";
        }

        EVENT_CLASS_TYPE(WindowFocusEvent, "WindowFocus", EventCategory::Application);
    };

    /// @brief Event emitted when the main window loses focus
    ///
    /// @warning Thread-safe emission - handlers executed on main thread via deferred processing
    ///
    /// WindowLostFocusEvent is triggered when the application window is no longer
    /// the active/focused window. This is useful for pausing game logic, muting
    /// audio, or performing other "background mode" activities.
    ///
    /// **Processing characteristics:**
    /// - Execution mode: Deferred (main thread for UI updates)
    /// - Priority: Normal (not critical but timely)
    /// - Main thread: Required (UI state management)
    /// - Typical handlers: Pause game logic, mute audio, hide cursor
    ///
    /// **Handler responsibilities:**
    /// - Pause game logic and timers
    /// - Reduce or mute audio volume
    /// - Disable input processing
    /// - Update UI to inactive state
    ///
    /// **Usage example:**
    /// @code
    /// auto subscription = g_EventSystem->Subscribe<WindowLostFocusEvent>(
    ///     [this](const WindowLostFocusEvent& e) {
    ///         m_AudioSystem->SetMasterVolume(0.0f);
    ///         m_GameLogic->Pause();
    ///     });
    /// @endcode
    class VA_API WindowLostFocusEvent final : public Event
    {
    public:
        /// @brief Construct a window lost focus event
        WindowLostFocusEvent()
            : Event(
                EventTraits<WindowLostFocusEvent>::Priority,
                EventTraits<WindowLostFocusEvent>::ExecutionMode)
        {
        }

        /// @brief Get debug string representation
        /// @return Human-readable description of the event
        std::string ToString() const override
        {
            return "WindowLostFocusEvent";
        }

        EVENT_CLASS_TYPE(WindowLostFocusEvent, "WindowLostFocus", EventCategory::Application);
    };

    /// @brief Event emitted when the main window is moved
    ///
    /// @warning Thread-safe emission - handlers executed on main thread via deferred processing
    ///
    /// WindowMovedEvent is triggered when the application window position changes
    /// on the desktop. This can be useful for saving window state, updating
    /// multi-monitor configurations, or adjusting rendering based on display.
    ///
    /// **Processing characteristics:**
    /// - Execution mode: Deferred (low-priority UI event)
    /// - Priority: Low (position changes are not critical)
    /// - Main thread: Required (UI state management)
    /// - Typical handlers: Save window state, update display configurations
    ///
    /// **Handler responsibilities:**
    /// - Save window position for restoration
    /// - Update multi-monitor display configurations
    /// - Adjust rendering for display-specific properties
    /// - Update window management state
    ///
    /// **Usage example:**
    /// @code
    /// auto subscription = g_EventSystem->Subscribe<WindowMovedEvent>(
    ///     [this](const WindowMovedEvent& e) {
    ///         SaveWindowPosition(e.GetX(), e.GetY());
    ///         CheckDisplayConfiguration();
    ///     });
    /// @endcode
    class VA_API WindowMovedEvent final : public Event
    {
    public:
        /// @brief Construct a window moved event
        /// @param x New window X position in screen coordinates
        /// @param y New window Y position in screen coordinates
        WindowMovedEvent(const int32_t x, const int32_t y)
            : Event(
                  EventTraits<WindowMovedEvent>::Priority,
                  EventTraits<WindowMovedEvent>::ExecutionMode),
              m_X(x),
              m_Y(y)
        {
        }

        /// @brief Get the new window X position
        /// @return Window X position in screen coordinates
        int32_t GetX() const { return m_X; }

        /// @brief Get the new window Y position
        /// @return Window Y position in screen coordinates
        int32_t GetY() const { return m_Y; }

        /// @brief Get debug string representation
        /// @return Human-readable description of the event with position
        std::string ToString() const override
        {
            return "WindowMovedEvent: " + std::to_string(m_X) + ", " + std::to_string(m_Y);
        }

        EVENT_CLASS_TYPE(WindowMovedEvent, "WindowMoved", EventCategory::Application);

    private:
        int32_t m_X; ///< New window X position in screen coordinates
        int32_t m_Y; ///< New window Y position in screen coordinates
    };
}
