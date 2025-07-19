//
// Created by Michael Desmedt on 17/07/2025.
//
#include "EventSystem.hpp"

#include "VoidArchitect/Engine/Common/Logger.hpp"
#include "VoidArchitect/Engine/Common/Systems/Jobs/JobSystem.hpp"

// Conditional moodycamel inclusion with fallback
#ifdef MOODYCAMEL_INCLUDE_DIR
#include "concurrentqueue.h"
#else
#include <mutex>
#include <queue>

// Fallback implementation when moodycamel is not available
namespace moodycamel
{
    template <typename T>
    class ConcurrentQueue
    {
        std::queue<T> m_Queue;
        mutable std::mutex m_Mutex;

    public:
        bool try_enqueue(const T& item)
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Queue.push(item);
            return true;
        }

        bool try_dequeue(T& item)
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            if (m_Queue.empty())
            {
                return false;
            }

            item = std::move(m_Queue.front());
            m_Queue.pop();
            return true;
        }

        size_t size_approx() const
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            return m_Queue.size();
        }
    };
}
#endif

namespace VoidArchitect::Events
{
    // =========================================================================
    // === Global Instance Definition ===
    // =========================================================================

    std::unique_ptr<EventSystem> g_EventSystem = nullptr;

    // =========================================================================
    // === EventSubscription Implementation ===
    // =========================================================================

    EventSubscription::EventSubscription(std::function<void()> unsubscribeFunc)
        : m_UnsubscribeFunc(std::move(unsubscribeFunc)),
          m_Valid(true)
    {
    }

    EventSubscription::~EventSubscription()
    {
        Unsubscribe();
    }

    EventSubscription::EventSubscription(EventSubscription&& other) noexcept
        : m_UnsubscribeFunc(std::move(other.m_UnsubscribeFunc)),
          m_Valid(other.m_Valid)
    {
        other.m_Valid = false;
    }

    EventSubscription& EventSubscription::operator=(EventSubscription&& other) noexcept
    {
        if (this != &other)
        {
            Unsubscribe();
            m_UnsubscribeFunc = std::move(other.m_UnsubscribeFunc);
            m_Valid = other.m_Valid;
            other.m_Valid = false;
        }

        return *this;
    }

    bool EventSubscription::IsValid() const
    {
        return m_Valid && m_UnsubscribeFunc != nullptr;
    }

    void EventSubscription::Unsubscribe()
    {
        if (IsValid())
        {
            m_UnsubscribeFunc();
            m_UnsubscribeFunc = nullptr;
            m_Valid = false;
        }
    }

    // =========================================================================
    // === EventSystem Implementation ===
    // =========================================================================

    EventSystem::EventSystem()
    {
        VA_ENGINE_INFO("[EventSystem] Initializing.");

        try
        {
            // Initialize deferred event queue
            m_DeferredQueue = std::make_unique<moodycamel::ConcurrentQueue<EventHandle>>();

            VA_ENGINE_INFO("[EventSystem] Initialized successfully.");
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_ERROR("[EventSystem] Failed to initialize: {}", e.what());
            throw;
        }
    }

    EventSystem::~EventSystem()
    {
        VA_ENGINE_INFO("[EventSystem] Shutting down...");

        try
        {
            // Process any remaining deferred events
            VA_ENGINE_INFO("[EventSystem] Processing deferred events...");
            ProcessDeferredEvents(); // No time limit for shutdown

            // Clean up all subscription
            {
                const std::unique_lock lock(m_Mutex);
                m_Subscriptions.clear();
                VA_ENGINE_INFO("[EventSystem] Cleared all subscriptions.");
            }

            // Note: FixedStorage destructor handles event clean up automatically

            const auto stats = GetStats();
            VA_ENGINE_INFO(
                "[EventSystem] Shutdown complete. Stats: {} events emitted, {} processed.",
                stats.totalEventsEmitted.load(),
                stats.totalEventsProcessed.load());
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_ERROR("[EventSystem] Failed to shutdown: {}", e.what());
        }
    }

    // === Core Processing Methods ===

    void EventSystem::QueueEventDeferred(const EventHandle eventHandle) const
    {
        if (!m_DeferredQueue->try_enqueue(eventHandle))
        {
            VA_ENGINE_WARN("[EventSystem] Deferred event queue is full. Event will be dropped.");
            return;
        }

        m_Stats.eventsInQueue.fetch_add(1, std::memory_order_relaxed);
    }

    void EventSystem::QueueEventAsync(EventHandle eventHandle)
    {
        // Submit a job to JobSystem for async processing
        auto job = [this, eventHandle]() -> Jobs::JobResult
        {
            ProcessSingleEvent(eventHandle);
            return Jobs::JobResult::Success();
        };

        auto jobHandle = Jobs::g_JobSystem->SubmitJob(std::move(job), "ProcessAsyncEvent");
        if (!jobHandle.IsValid())
        {
            VA_ENGINE_WARN("[EventSystem] Failed to submit async event processing job.");
        }
    }

    // Note: This method can be called by multiple threads (used into jobs)
    void EventSystem::ProcessSingleEvent(EventHandle eventHandle)
    {
        auto* event = m_EventStorage.Get(eventHandle);
        if (!event)
        {
            VA_ENGINE_WARN("[EventSystem] Attempted to process invalid event handle.");
            return;
        }

        // Mark processing started
        event->MarkProcessingStarted();

        auto startTime = std::chrono::high_resolution_clock::now();

        try
        {
            // Get subscriptions for this event type
            auto subscriptions = GetSubscriptionsForType(event->GetEventTypeID());

            // Process all handlers
            for (const auto& subscription : subscriptions)
            {
                if (subscription.active && subscription.handler)
                {
                    subscription.handler(*event);
                }
            }

            // Update statistics
            auto endTime = std::chrono::high_resolution_clock::now();
            auto processingTime = std::chrono::duration_cast<std::chrono::microseconds>(
                endTime - startTime).count();

            m_Stats.processingTimes.totalProcessingTime.fetch_add(
                processingTime,
                std::memory_order_relaxed);

            // Update min/max processing times
            auto currentMin = m_Stats.processingTimes.minProcessingTime.load(
                std::memory_order_relaxed);
            while (processingTime < currentMin)
            {
                if (m_Stats.processingTimes.minProcessingTime.compare_exchange_weak(
                    currentMin,
                    processingTime,
                    std::memory_order_relaxed))
                {
                    break;
                }
            }

            auto currentMax = m_Stats.processingTimes.maxProcessingTime.load(
                std::memory_order_relaxed);
            while (processingTime > currentMax)
            {
                if (m_Stats.processingTimes.maxProcessingTime.compare_exchange_weak(
                    currentMax,
                    processingTime,
                    std::memory_order_relaxed))
                {
                    break;
                }
            }
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_ERROR("[EventSystem] Failed to process event: {}", e.what());
        }

        // Mark processing completed
        event->MarkProcessingCompleted();
        m_Stats.totalEventsProcessed.fetch_add(1, std::memory_order_relaxed);
    }

    EventSystem::DeferredEventStats EventSystem::ProcessDeferredEvents(float maxTimeMs)
    {
        DeferredEventStats stats;
        auto startTime = std::chrono::high_resolution_clock::now();

        EventHandle eventHandle;
        while (m_DeferredQueue->try_dequeue(eventHandle))
        {
            // Check time budget if specified
            if (maxTimeMs > 0.0f)
            {
                auto currentTime = std::chrono::high_resolution_clock::now();
                auto elapsedMs = std::chrono::duration<float, std::milli>(currentTime - startTime).
                    count();
                if (elapsedMs > maxTimeMs)
                {
                    // Re-queue the event we just dequeued
                    m_DeferredQueue->try_enqueue(eventHandle);
                    stats.budgetExceeded = (stats.eventsProcessed > 0); // Only flag if we had work
                    break;
                }
            }

            // Get event priority for statistics
            auto* event = m_EventStorage.Get(eventHandle);
            if (event)
            {
                auto priorityIndex = static_cast<uint32_t>(event->GetPriority());
                if (priorityIndex < 4)
                {
                    stats.eventsByPriority[priorityIndex]++;
                }
            }

            // Process the event
            ProcessSingleEvent(eventHandle);
            stats.eventsProcessed++;

            // Update queue statitics
            m_Stats.eventsInQueue.fetch_sub(1, std::memory_order_relaxed);
        }

        // Calculate final time spent
        auto endTime = std::chrono::high_resolution_clock::now();
        stats.timeSpentMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();

        return stats;
    }

    void EventSystem::BeginFrame()
    {
        // === Frame Statistics and Monitoring ===

        // Check for potential performance issues
        auto queueSize = m_DeferredQueue->size_approx();
        if (queueSize > 1000) // Threshold configurable
        {
            VA_ENGINE_WARN(
                "[EventSystem] High deferred event queue size: {} events pending",
                queueSize);
        }

        // Update queue size statistics
        m_Stats.eventsInQueue.store(queueSize, std::memory_order_relaxed);

        // === Optional: Periodic Debug Logging ===

        static uint32_t frameCount = 0;
        if (++frameCount % 300 == 0) // Every 5 seconds at 60 FPS
        {
            auto stats = GetStats();
            VA_ENGINE_TRACE(
                "[EventSystem] Frame {}: {} events emitted this session, {} queued, {} subscriptions active",
                frameCount,
                stats.totalEventsEmitted.load(),
                stats.eventsInQueue.load(),
                stats.activeSubscriptions.load());
        }

        // === Event Storage Maintenance ===
        // Note: FixedStorage handles cleanup automatically, but we could add pressure monitoring

        auto usedSlots = m_EventStorage.GetUsedSlots();
        auto totalSlots = MAX_EVENTS;
        auto usagePercent = (usedSlots * 100) / totalSlots;

        if (usagePercent > 80) // High memory pressure
        {
            VA_ENGINE_WARN(
                "[EventSystem] High event storage usage: {}/{} slots ({}%)",
                usedSlots,
                totalSlots,
                usagePercent);
        }
    }

    EventSystemStats EventSystem::GetStats() const
    {
        return m_Stats;
    }

    bool EventSystem::HasPendingDeferredEvents() const
    {
        return m_DeferredQueue->size_approx() > 0;
    }

    uint32_t EventSystem::GetActiveSubscriptionCount() const
    {
        return m_Stats.activeSubscriptions.load(std::memory_order::relaxed);
    }

    // === Private Helper Methods ===

    VAArray<EventSystem::SubscriptionEntry> EventSystem::GetSubscriptionsForType(
        EventTypeID typeId) const
    {
        const std::shared_lock lock(m_Mutex);

        auto it = m_Subscriptions.find(typeId);
        if (it != m_Subscriptions.end())
        {
            return it->second;
        }

        return {};
    }

    void EventSystem::RemoveSubscription(EventTypeID typeId, uint32_t subscriptionId)
    {
        const std::unique_lock lock(m_Mutex);

        auto it = m_Subscriptions.find(typeId);
        if (it != m_Subscriptions.end())
        {
            auto& subscriptions = it->second;

            // Find and remove the sub
            std::erase_if(
                subscriptions,
                [subscriptionId](const auto& entry)
                {
                    return entry.subscriptionId == subscriptionId;
                });

            // Remove type entry if no subscriptions remain
            if (subscriptions.empty())
            {
                m_Subscriptions.erase(it);
            }

            // Update stats
            m_Stats.activeSubscriptions.fetch_sub(1, std::memory_order_relaxed);

            VA_ENGINE_TRACE(
                "[EventSystem] Removed subscription {} for event type {}.",
                subscriptionId,
                typeId);
        }
    }

    uint32_t EventSystem::GenerateSubscriptionId()
    {
        return m_NextSubscriptionId.fetch_add(1, std::memory_order_relaxed);
    }
}
