//
// Created by Michael Desmedt on 13/05/2025.
//
#pragma once

#include "EventTypes.hpp"

#include "VoidArchitect/Engine/Common/Core.hpp"
#include "VoidArchitect/Engine/Common/Platform/Threading/Thread.hpp"

namespace VoidArchitect::Events
{
    /// @brief Modern event base class for the VoidArchitect event system
    ///
    /// Provides a comprehensive event foundation with metadata, timing information,
    /// and debugging capabilities. This replaces the legacy Event class to support
    /// modern event system features including
    /// - Automatic type identification and validation
    /// - Threading context tracking for debugging
    /// - Timing information for performance profiling
    /// - Source location tracking for debugging
    /// - Priority and execution mode configuration
    ///
    /// **Event lifecycle: **
    /// 1. Event created via EmitEvent<T>() with metadata
    /// 2. Queued based on execution mode (Immediate/Deferred/Async)
    /// 3. Processed by the appropriate handler with timing tracking
    /// 4. Destroyed or returned to pool for reuse
    ///
    /// **Thread safety: **
    /// - Event objects are immutable after creation
    /// - Safe to read from multiple threads simultaneously
    /// - Metadata includes thread context for debugging
    ///
    /// **Memory management: **
    /// - Events are pooled to avoid allocation overhead
    /// - Automatic clean-up via EventSystem
    /// - Handle-based references prevent use-after-free
    ///
    /// @see EventSystem for event emission and subscription
    /// @see EventTraits for compile-time event configuration
    class VA_API Event
    {
    public:
        virtual ~Event() = default;

        // === Type Identification ===

        /// @brief Get the unique type ID for this event
        /// @return Event type identifier for dispatch and filtering
        [[nodiscard]] virtual EventTypeID GetEventTypeID() const = 0;

        /// @brief Get a human-readable name for debugging
        /// @return Event type name as string (should be static string)
        [[nodiscard]] virtual const char* GetEventName() const = 0;

        /// @brief Get event category flags for filtering
        /// @return Bitfield of event categories this event belongs to
        [[nodiscard]] virtual EventCategory GetCategoryFlags() const = 0;

        /// @brief Check if an event belongs to a specific category
        /// @param category Category to check against
        /// @return true if the event belongs to the specified category
        [[nodiscard]] virtual bool IsInCategory(EventCategory category) const
        {
            return (GetCategoryFlags() & category) != 0;
        }

        // === Event Metadata Access ===

        /// @brief Get an event handle for this event instance
        /// @return Handle to this event for reference tracking
        EventHandle GetHandle() const { return m_Handle; }

        /// @brief Get event emission timestamp
        /// @return High-resolution timestamp when the event was created
        std::chrono::high_resolution_clock::time_point GetTimestamp() const { return m_Timestamp; }

        /// @brief Get event priority level
        /// @return Priority level for processing order
        EventPriority GetPriority() const { return m_Priority; }

        /// @brief Get event execution mode
        /// @return Execution mode (Immediate/Deferred/Async)
        EventExecutionMode GetExecutionMode() const { return m_ExecutionMode; }

        /// @brief Get thread ID that emitted this event
        /// @return Thread handle for debugging and context tracking
        Platform::ThreadHandle GetEmitterThread() const { return m_EmitterThread; }

        /// @brief Get source file where event was emitted
        /// @return Source file path (maybe nullptr)
        const char* GetSourceFile() const { return m_SourceFile; }

        /// @brief Get source line where event was emitted
        /// @return Source line number (0 if not available)
        int GetSourceLine() const { return m_SourceLine; }

        /// @brief Check if an event has been processed
        /// @return true if event processing has completed
        bool IsProcessed() const { return m_Processed.load(std::memory_order_acquire); }

        // === Debug and Profiling ===

        /// @brief Get debug string representation of the event
        /// @return Human-readable event description for logging
        ///
        /// Default implementation returns the event name. Derived classes should
        /// override to provide more detailed information, including event data.
        virtual std::string ToString() const
        {
            return {GetEventName()};
        }

        /// @brief Get processing duration if the event has been processed
        /// @return Processing time in microseconds (0 if not processed)
        std::chrono::microseconds GetProcessingDuration() const
        {
            if (!IsProcessed())
            {
                return std::chrono::microseconds(0);
            }

            return std::chrono::duration_cast<std::chrono::microseconds>(
                m_ProcessingEndTime - m_ProcessingStartTime);
        }

    protected:
        /// @brief Protected constructor for derived classes
        /// @param priority Event priority level
        /// @param executionMode Event execution mode
        ///
        /// Automatically captures the emission timestamp and thread context.
        /// Should only be called by EventSystem during event creation.
        Event(const EventPriority priority, const EventExecutionMode executionMode)
            : m_Handle(InvalidEventHandle),
              m_Timestamp(std::chrono::high_resolution_clock::now()),
              m_Priority(priority),
              m_ExecutionMode(executionMode),
              m_EmitterThread(Platform::Thread::GetCurrentThreadHandle()),
              m_SourceFile(nullptr),
              m_SourceLine(0),
              m_Processed(false)
        {
        }

    private:
        // === Event System Internal Access ===
        friend class EventSystem;

        /// @brief Set the event handle (called by EventSystem)
        /// @param handle Handle to assign to this event
        void SetHandle(const EventHandle handle) { m_Handle = handle; }

        /// @brief Set source location information
        /// @param sourceFile Source file where the event was emitted
        /// @param sourceLine Source line where the event was emitted
        void SetSourceLocation(const char* sourceFile, const int sourceLine)
        {
            m_SourceFile = sourceFile;
            m_SourceLine = sourceLine;
        }

        /// @brief Mark the event as processing started (called by EventSystem)
        void MarkProcessingStarted()
        {
            m_ProcessingStartTime = std::chrono::high_resolution_clock::now();
        }

        /// @brief Mark the event as processing completed (called by EventSystem)
        void MarkProcessingCompleted()
        {
            m_ProcessingEndTime = std::chrono::high_resolution_clock::now();
            m_Processed.store(true, std::memory_order_release);
        }

        // === Event Instance Data ===

        /// @brief Handle to this event instance
        EventHandle m_Handle;

        /// @brief Timestamp when event was created
        std::chrono::high_resolution_clock::time_point m_Timestamp;

        /// @brief Event priority for processing order
        EventPriority m_Priority;

        /// @brief Event execution mode
        EventExecutionMode m_ExecutionMode;

        /// @brief Thread that emitted this event
        Platform::ThreadHandle m_EmitterThread;

        /// @brief Source file where the event was emitted (optional debug info)
        const char* m_SourceFile;

        /// @brief Source line where the event was emitted (optional debug info)
        int m_SourceLine;

        /// @brief Processing completion flag
        std::atomic<bool> m_Processed;

        /// @brief Processing timing information
        std::chrono::high_resolution_clock::time_point m_ProcessingStartTime;
        std::chrono::high_resolution_clock::time_point m_ProcessingEndTime;
    };

    // === Event Traits System ===

    /// @brief Default event configuration traits
    /// @tparam T Event type
    ///
    /// Provides compile-time configuration for event types. Derived classes
    /// can specialize this template to override default behaviour.
    template <typename T>
    struct EventTraits
    {
        /// @brief Default execution mode for this event type
        static constexpr EventExecutionMode ExecutionMode = EventExecutionMode::Deferred;

        /// @brief Default priority for this event type
        static constexpr EventPriority Priority = EventPriority::Normal;

        /// @brief Whether this event type requires main thread processing
        static constexpr bool RequiresMainThread = false;

        /// @brief Default category flags for this event type
        static constexpr EventCategory CategoryFlags = EventCategory::None;
    };

    // === Event Type Macros ===

    /// @brief Macro to define event type identification methods
    /// @param EventClass Event class name
    /// @param EventName Human-readable event name
    /// @param Category Event category flags
    ///
    /// Provides standard implementation of type identification methods
    /// required by the Event base class.
#define EVENT_CLASS_TYPE(EventClass, EventName, Category) \
    static EventTypeID GetStaticTypeID() { return Events::GetEventTypeID<EventClass>(); } \
    inline EventTypeID GetEventTypeID() const override { return GetStaticTypeID(); } \
    inline const char* GetEventName() const override { return EventName; } \
    inline EventCategory GetCategoryFlags() const override { return static_cast<EventCategory>(Category); }

    /// @brief Macro to define event traits specialization
    /// @param EventClass Event class name
    /// @param ExecMode Execution mode
    /// @param Prio Priority level
    /// @param MainThread Whether the main thread is required
    /// @param Cat Category flags
    ///
    /// Provide convenient specialization of the EventTraits template.
#define EVENT_TRAITS(EventClass, ExecMode, Prio, MainThread, Cat) \
    template <> struct EventTraits<EventClass> { \
        static constexpr EventExecutionMode ExecutionMode = ExecMode; \
        static constexpr EventPriority Priority = Prio; \
        static constexpr bool RequiresMainThread = MainThread; \
        static constexpr EventCategory CategoryFlags = static_cast<EventCategory>(Cat); \
    };
}
