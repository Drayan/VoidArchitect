//
// Created by Michael Desmedt on 21/06/2025.
//
#include "SyncPoint.hpp"

#include "JobTypes.hpp"
#include "JobTypes.hpp"

namespace VoidArchitect::Jobs
{
    SyncPoint::SyncPoint(uint32_t initialCount, const char* name)
        : counter(initialCount),
          status(JobResult::Status::Success),
          debugName(name),
          creationTime(std::chrono::high_resolution_clock::now())
    {
        // Initialize inline continuations to invalid handles
        for (auto& continuation : inlineContinuations)
        {
            continuation.store(InvalidJobHandle, std::memory_order_relaxed);
        }
    }

    SyncPoint::~SyncPoint()
    {
        auto overflow = overflowContinuations.load(std::memory_order_acquire);
        if (overflow)
        {
            delete overflow;
        }
    }

    bool SyncPoint::DecrementAndCheck(const JobResult& result)
    {
        // Propagate status if this job failed/cancelled
        if (result.status != JobResult::Status::Success)
        {
            PropagateFailure(result.status);
        }

        // Atomically decrement and check if we were the last dependency
        auto previousCount = counter.fetch_sub(1, std::memory_order_acq_rel);
        return previousCount == 1;
    }

    bool SyncPoint::AddContinuation(JobHandle handle)
    {
        auto currentCount = inlineCount.load(std::memory_order::relaxed);

        while (currentCount < INLINE_CONTINUATIONS)
        {
            // Try to claim a slot atomically
            if (inlineCount.compare_exchange_weak(
                currentCount,
                currentCount + 1,
                std::memory_order_acq_rel,
                std::memory_order::relaxed))
            {
                // Succesfully claimed slot - store the handle
                inlineContinuations[currentCount].store(handle, std::memory_order::release);
                return true;
            }

            // CAS failed, currentCount was updated - retry
        }

        // Slow path: overflow storage (rare case)
        return AddToOverflow(handle);
    }

    VAArray<JobHandle> SyncPoint::GetContinuations()
    {
        VAArray<JobHandle> allContinuations;

        // Collect inline continuations
        auto count = inlineCount.load(std::memory_order_acquire);
        for (uint8_t i = 0; i < count && i < INLINE_CONTINUATIONS; ++i)
        {
            auto handle = inlineContinuations[i].load(std::memory_order_acquire);
            if (handle.IsValid())
            {
                allContinuations.push_back(handle);
            }
        }

        // Collect overflow continuations if they exists
        auto* overflow = overflowContinuations.load(std::memory_order_acquire);
        if (overflow)
        {
            // Acquire spinlock for reading
            while (overflowLock.exchange(true, std::memory_order_acquire))
            {
                Platform::Thread::Yield();
            }

            // Copy overflow continuations
            allContinuations.insert(allContinuations.end(), overflow->begin(), overflow->end());

            // Release spinlock
            overflowLock.store(false, std::memory_order_release);
        }

        return allContinuations;
    }

    void SyncPoint::PropagateFailure(JobResult::Status failureStatus)
    {
        auto expected = JobResult::Status::Success;
        status.compare_exchange_strong(expected, failureStatus, std::memory_order_acq_rel);
        // If CAS failed, status was already degraded by another job - that's fine.
    }

    bool SyncPoint::AddToOverflow(JobHandle handle)
    {
        auto* currentOverflow = overflowContinuations.load(std::memory_order_acquire);

        // If no overflow vector exists, create one
        if (!currentOverflow)
        {
            auto* newOverflow = new VAArray<JobHandle>();
            newOverflow->reserve(4); // Start small
            newOverflow->push_back(handle);

            // Try to install the new vector atomically
            VAArray<JobHandle>* expected = nullptr;
            if (overflowContinuations.compare_exchange_strong(
                expected,
                newOverflow,
                std::memory_order_acq_rel))
            {
                // Successfully installed new overflow vector
                return true;
            }
            else
            {
                delete newOverflow;
                currentOverflow = expected;
            }
        }

        // Overflow vector exists - acquire spinlock and add to it
        while (overflowLock.exchange(true, std::memory_order_acquire))
        {
            Platform::Thread::Yield();
        }

        currentOverflow->push_back(handle);

        // Release Spinlock
        overflowLock.store(false, std::memory_order_release);
        return true;
    }
}
