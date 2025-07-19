//
// Created by Michael Desmedt on 16/07/2025.
//
#pragma once

#include "VoidArchitect/Engine/Common/Core.hpp"
#include "VoidArchitect/Engine/Common/Handle.hpp"
#include "VoidArchitect/Engine/Common/Systems/Jobs/JobTypes.hpp"
#include "VoidArchitect/Engine/Common/Utils.hpp"

namespace VoidArchitect::Events
{
    // === Forward Declarations ===
    class Event;
    class EventSystem;

    // === Configuration Constants ===

    /// @brief Maximum number of events that can exist simultaneously in the system
    constexpr size_t MAX_EVENTS = 8192;

    /// @brief Maximum number of event subscriptions per event type
    constexpr size_t MAX_SUBSCRIPTIONS_PER_TYPE = 256;

    /// @brief Maximum number of different event types supported
    constexpr size_t MAX_EVENT_TYPES = 512;

    /// @brief  Maximum size for any event type in bytes
    /// This must be large enough to hold the largest event class
    constexpr size_t MAX_EVENT_SIZE = 128;

    // === Handle Types ===

    /// @brief Handle for referencing events with generation-based validation
    using EventHandle = Handle<Event>;

    /// @brief Invalid event handle constant for convenience
    constexpr EventHandle InvalidEventHandle = EventHandle::Invalid();

    /// @brief Type identifier for event types with collision detection
    using EventTypeID = uint32_t;

    /// @brief Invalid event type ID constant
    constexpr EventTypeID InvalidEventTypeID = std::numeric_limits<uint32_t>::max();

    // === Event Execution Modes ===

    /// @brief Defines how and when events are processed
    ///
    /// Controls the execution timing and threading behaviour for event processing:
    /// - Immediate: Processed synchronously in the emitting thread
    /// - Deferred: Queued for main thread processing (via JobSystem MAIN_THREAD_ONLY)
    /// - Async: Queued for worker thread processing (via JobSystem ANY_WORKER)
    enum class EventExecutionMode : uint8_t
    {
        /// @brief Process immediately in the emitting thread
        ///
        /// Best for critical event that requires immediate handling.
        Immediate = 0,

        /// @brief Queue for main thread processing
        ///
        /// Best for events requiring main thread context.
        Deferred = 1,

        /// @brief Queue for worker thread processing
        ///
        /// Best for heavy processing that can run on any thread:
        /// - Network message processing
        /// - File I/O operation
        /// - Data processing and calculations
        Async = 2,
    };

    // === Event Priority ===

    /// @brief Event processing priority levels
    ///
    /// Maps directly to JobSystem priority levels for consistent scheduling
    /// across the entire engine. Higher priority events are processed first.
    using EventPriority = Jobs::JobPriority;

    // === Event Categories ===

    /// @brief Event categories for filtering and debugging
    ///
    /// Bitfield-based categories allow events to belong to multiple categories
    /// for flexible filtering and debugging capabilities.
    enum EventCategory : uint32_t
    {
        None = 0,
        Application = BIT(0), ///< Application lifecycle events
        Input = BIT(1), ///< Input device events
        Keyboard = BIT(2), ///< Keyboard-specific events
        Mouse = BIT(3), ///< Mouse-specific events
        Network = BIT(4), ///< Network communication events
        Rendering = BIT(5), ///< Rendering system events
        Audio = BIT(6), ///< Audio system events
        Physics = BIT(7), ///< Physics simulation events
        Game = BIT(8), ///< Game logic events
        Editor = BIT(9), ///< Editor-specific events
        Debug = BIT(10) ///< Debug and profiling events
    };

    // === Event Statistics ===

    /// @brief Statistics structure for event system performance monitoring
    ///
    /// Provides comprehensive metrics for profiling and debugging the event system.
    /// All counters are atomic for thread-safe access from monitoring threads.
    struct EventSystemStats
    {
        // === Constructors ===

        EventSystemStats() = default;

        /// @brief Copy constructor for atomic members
        /// @param other Source stats to copy from
        EventSystemStats(const EventSystemStats& other)
            : totalEventsEmitted(other.totalEventsEmitted.load(std::memory_order_relaxed)),
              totalEventsProcessed(other.totalEventsProcessed.load(std::memory_order_relaxed)),
              eventsInQueue(other.eventsInQueue.load(std::memory_order_relaxed)),
              eventsBeingProcessed(other.eventsBeingProcessed.load(std::memory_order_relaxed)),
              activeSubscriptions(other.activeSubscriptions.load(std::memory_order_relaxed)),
              processingTimes(other.processingTimes),
              eventsByMode(other.eventsByMode),
              eventsByPriority(other.eventsByPriority)
        {
        }

        EventSystemStats& operator=(const EventSystemStats& other)
        {
            if (this != &other)
            {
                totalEventsEmitted = other.totalEventsEmitted.load(std::memory_order_relaxed);
                totalEventsProcessed = other.totalEventsProcessed.load(std::memory_order_relaxed);
                eventsInQueue = other.eventsInQueue.load(std::memory_order_relaxed);
                eventsBeingProcessed = other.eventsBeingProcessed.load(std::memory_order_relaxed);
                activeSubscriptions = other.activeSubscriptions.load(std::memory_order_relaxed);
                processingTimes = other.processingTimes;
                eventsByMode = other.eventsByMode;
                eventsByPriority = other.eventsByPriority;
            }

            return *this;
        }

        /// @brief Total events emitted since system initialization
        std::atomic<uint64_t> totalEventsEmitted{0};

        /// @brief Total events processed since system initialization
        std::atomic<uint64_t> totalEventsProcessed{0};

        /// @brief Events currently queued for processing
        std::atomic<uint64_t> eventsInQueue{0};

        /// @brief Events currently being processed
        std::atomic<uint64_t> eventsBeingProcessed{0};

        /// @brief Current number of active subscriptions across all event types
        std::atomic<uint64_t> activeSubscriptions{0};

        /// @brief Event processing time statistics (microseconds)
        struct ProcessingTimes
        {
            ProcessingTimes() = default;

            ProcessingTimes(const ProcessingTimes& other)
                : totalProcessingTime(other.totalProcessingTime.load(std::memory_order_relaxed)),
                  maxProcessingTime(other.maxProcessingTime.load(std::memory_order_relaxed)),
                  minProcessingTime(other.minProcessingTime.load(std::memory_order_relaxed))
            {
            }

            ProcessingTimes& operator=(const ProcessingTimes& other)
            {
                if (this != &other)
                {
                    totalProcessingTime = other.totalProcessingTime.load(std::memory_order_relaxed);
                    maxProcessingTime = other.maxProcessingTime.load(std::memory_order_relaxed);
                    minProcessingTime = other.minProcessingTime.load(std::memory_order_relaxed);
                }

                return *this;
            }

            std::atomic<uint64_t> totalProcessingTime{0};
            std::atomic<uint64_t> maxProcessingTime{0};
            std::atomic<uint64_t> minProcessingTime{std::numeric_limits<uint64_t>::min()};
        } processingTimes;

        /// @brief Events processed by execution mode
        struct ByExecutionMode
        {
            ByExecutionMode() = default;

            ByExecutionMode(const ByExecutionMode& other)
                : immediate(other.immediate.load(std::memory_order_relaxed)),
                  deferred(other.deferred.load(std::memory_order_relaxed)),
                  async(other.async.load(std::memory_order_relaxed))
            {
            }

            ByExecutionMode& operator=(const ByExecutionMode& other)
            {
                if (this != &other)
                {
                    immediate = other.immediate.load(std::memory_order_relaxed);
                    deferred = other.deferred.load(std::memory_order_relaxed);
                    async = other.async.load(std::memory_order_relaxed);
                }

                return *this;
            }

            std::atomic<uint64_t> immediate{0};
            std::atomic<uint64_t> deferred{0};
            std::atomic<uint64_t> async{0};
        } eventsByMode;

        /// @brief Events processed by priority level
        struct ByPriority
        {
            ByPriority() = default;

            ByPriority(const ByPriority& other)
                : low(other.low.load(std::memory_order_relaxed)),
                  normal(other.normal.load(std::memory_order_relaxed)),
                  high(other.high.load(std::memory_order_relaxed)),
                  critical(other.critical.load(std::memory_order_relaxed))
            {
            }

            ByPriority& operator=(const ByPriority& other)
            {
                if (this != &other)
                {
                    low = other.low.load(std::memory_order_relaxed);
                    normal = other.normal.load(std::memory_order_relaxed);
                    high = other.high.load(std::memory_order_relaxed);
                    critical = other.critical.load(std::memory_order_relaxed);
                }

                return *this;
            }

            std::atomic<uint64_t> low{0};
            std::atomic<uint64_t> normal{0};
            std::atomic<uint64_t> high{0};
            std::atomic<uint64_t> critical{0};
        } eventsByPriority;
    };

    // === Event Subscription Management ===

    class VA_API EventSubscription
    {
    public:
        /// @brief Default constructor creates an invalid subscription
        EventSubscription() = default;

        /// @brief Construct a valid subscription with a clean-up function
        /// @param unsubscribeFunc Function to call for clean-up.
        explicit EventSubscription(std::function<void()> unsubscribeFunc);

        /// @brief Destructor automatically unsubscribes
        ~EventSubscription();

        // Non-copyable but movable for RAII semantics
        EventSubscription(const EventSubscription&) = delete;
        EventSubscription& operator=(const EventSubscription&) = delete;
        EventSubscription(EventSubscription&&) noexcept;
        EventSubscription& operator=(EventSubscription&&) noexcept;

        /// @brief Check if this subscription is valid and active
        /// @return True, if the subscription is active
        bool IsValid() const;

        /// @brief Manually unsubscribe (optional)
        void Unsubscribe();

    private:
        /// @brief Function to call for clean-up
        std::function<void()> m_UnsubscribeFunc;

        /// @brief Validity flag for moved-from objects
        bool m_Valid = false;
    };

    // === Event Type Registration ===

    // @brief Internal function to generate unique event type IDs
    /// @tparam T Event type
    /// @return Generated unique type ID
    ///
    /// Implementation uses VoidArchitect's HashCombine utility for consistent
    /// hashing across the engine. Generates stable type IDs across compilation units.
    template <typename T>
    EventTypeID GenerateEventTypeID()
    {
        size_t hash = 0;
        HashCombine(hash, std::string_view(typeid(T).name()));
        const auto typeId = static_cast<EventTypeID>(hash);
        return typeId == InvalidEventTypeID ? 1 : typeId;
    }

    /// @brief Template function to get unique type ID for event types
    /// @tparam T Event type
    /// @return Unique type ID for the event type
    ///
    /// Uses static local variable to ensure each event type gets a unique ID.
    /// Thread-safe initialization guaranteed by C++11 static local semantics.
    template <typename T>
    EventTypeID GetEventTypeID()
    {
        static_assert(std::is_base_of_v<Event, T>, "T must derive from Event");
        static const auto typeId = GenerateEventTypeID<T>();
        return typeId;
    }
}
