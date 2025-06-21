//
// Created by Michael Desmedt on 20/06/2025.
//
#include "JobScheduler.hpp"

#include <random>

#include "Core/Logger.hpp"

// Conditional moodycamel inclusion with fallback
#ifdef MOODYCAMEL_INCLUDE_DIR
#include "concurrentqueue.h"
#else
// Fallback to the standard queue with mutex protection
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

namespace VoidArchitect::Jobs
{
    // ==============================================================
    // === JobScheduler Implementation ===
    // ==============================================================

    JobScheduler::JobScheduler(uint32_t workerCount)
    {
        VA_ENGINE_INFO("[JobScheduler] Initializing with {} workers.", workerCount);

        // Determine worker count
        if (workerCount == 0)
        {
            const uint32_t hwThreads = Platform::ThreadFactory::GetHardwareConcurrency();
            workerCount = std::max(1u, hwThreads - 1);
            VA_ENGINE_INFO(
                "[JobScheduler] Auto-detected {} workers (hw={}, main thread reserved).",
                workerCount,
                hwThreads);
        }
        else
        {
            VA_ENGINE_INFO("[JobScheduler] Using {} workers.", workerCount);
        }

        // Validate worker count
        if (workerCount > MAX_WORKERS)
        {
            VA_ENGINE_ERROR(
                "[JobScheduler] Cannot initialize with {} workers - max is {}.",
                workerCount,
                MAX_WORKERS);
            throw std::runtime_error("JobScheduler: Invalid worker count.");
        }

        // Reset shutdown flag
        m_Shutdown.store(false, std::memory_order_release);

        try
        {
            // Initialize priority queues
            InitializePriorityQueues();

            // Initialize worker threads
            InitializeWorkers(workerCount);

            VA_ENGINE_INFO(
                "[JobScheduler] Job scheduler initialized successfully with {} workers.",
                workerCount);
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_ERROR("[JobScheduler] Failed to initialize: {}", e.what());
            CleanupResources();
            throw;
        }
    }

    JobScheduler::~JobScheduler()
    {
        VA_ENGINE_INFO("[JobScheduler] Shutting down...");

        // Signal shutdown to all workers
        m_Shutdown.store(true, std::memory_order_release);

        // Wait for all workers to finish
        for (const auto& worker : m_Workers)
        {
            if (worker && worker->IsJoinable())
            {
                worker->Join();
            }
        }

        // Clean up resources
        CleanupResources();

        VA_ENGINE_INFO("[JobScheduler] Shutdown successfully.");
    }

    // === SyncPoint Management ===

    SyncPointHandle JobScheduler::CreateSyncPoint(uint32_t initialCount, const char* name)
    {
        auto handle = m_SyncPointStorage.Allocate(initialCount, name);
        if (!handle.IsValid())
        {
            VA_ENGINE_ERROR(
                "[JobScheduler] Failed to create sync point '{}' - storage full.",
                name);
            return InvalidSyncPointHandle;
        }

        m_Stats.syncPointsCreated.fetch_add(1, std::memory_order_relaxed);

        VA_ENGINE_DEBUG(
            "[JobScheduler] Created sync point '{}' with {} dependencies.",
            name,
            initialCount);
        return handle;
    }

    void JobScheduler::Signal(const SyncPointHandle sp, const JobResult result)
    {
        auto syncPoint = m_SyncPointStorage.Get(sp);
        if (!syncPoint)
        {
            VA_ENGINE_WARN("[JobScheduler] Signal called with invalid sync point handle.");
            return;
        }

        if (syncPoint->DecrementAndCheck(result))
        {
            ProcessSyncPointCompletion(syncPoint);
        }
    }

    void JobScheduler::Cancel(SyncPointHandle sp, const char* reason)
    {
        auto syncPoint = m_SyncPointStorage.Get(sp);
        if (!syncPoint)
        {
            VA_ENGINE_WARN("[JobScheduler] Cancel called with invalid sync point handle.");
            return;
        }

        VA_ENGINE_INFO(
            "[JobScheduler] Cancelling sync point '{}' : {}",
            syncPoint->GetDebugName(),
            reason ? reason : "No reason specified");

        // Force status to cancelled and trigger completion
        auto cancelResult = JobResult::Cancelled(reason);

        // Set counter to 0 and process completion
        auto expectedCount = syncPoint->counter.exchange(0, std::memory_order_acq_rel);
        if (expectedCount > 0)
        {
            syncPoint->status.store(cancelResult.status, std::memory_order_release);
            ProcessSyncPointCompletion(syncPoint);
        }
    }

    bool JobScheduler::IsSignaled(SyncPointHandle sp) const
    {
        const auto syncPoint = m_SyncPointStorage.Get(sp);
        return syncPoint ? syncPoint->IsSignaled() : false;
    }

    JobResult::Status JobScheduler::GetSyncPointStatus(SyncPointHandle sp) const
    {
        const auto syncPoint = m_SyncPointStorage.Get(sp);
        return syncPoint ? syncPoint->GetStatus() : JobResult::Status::Failed;
    }

    // === Job Submission ===

    JobHandle JobScheduler::Submit(
        JobFunction job,
        SyncPointHandle signalSP,
        JobPriority priority,
        const char* name)
    {
        // Check backpressure
        auto backpressure = CheckBackpressure();
        if (backpressure == SubmissionResult::StorageFull_Critical)
        {
            VA_ENGINE_ERROR(
                "[JobScheduler] Cannot submit job '{}' - critical backpressure.",
                name ? name : "Unnamed");

            return InvalidJobHandle;
        }

        // Allocate job slot
        auto handle = m_JobStorage.Allocate(std::move(job), signalSP, name, priority);
        if (!handle.IsValid())
        {
            VA_ENGINE_ERROR("[JobScheduler] Failed to allocate job slot for '{}'.", name);
            return InvalidJobHandle;
        }

        // Enqueue job for execution
        EnqueueJob(handle, priority);

        m_Stats.jobsSubmitted.fetch_add(1, std::memory_order_relaxed);

        VA_ENGINE_TRACE(
            "[JobScheduler] Submitted job '{}' (handle: {}) with priority {}.",
            name,
            handle.GetPacked(),
            GetPriorityString(priority));

        return handle;
    }

    JobHandle JobScheduler::SubmitAfter(
        SyncPointHandle dependency,
        JobFunction job,
        SyncPointHandle signalSP,
        JobPriority priority,
        const char* name)
    {
        // Check if the dependency is already signalled
        if (IsSignaled(dependency))
        {
            auto depStatus = GetSyncPointStatus(dependency);
            if (depStatus == JobResult::Status::Success)
            {
                // Job is already complete, submit job immediately
                return Submit(std::move(job), signalSP, priority, name);
            }
            else
            {
                VA_ENGINE_DEBUG(
                    "[JobScheduler] Job '{}' cancelled due to failed dependency.",
                    name ? name : "Unnamed");
                Signal(signalSP, JobResult::Cancelled("Dependency failed"));

                return InvalidJobHandle;
            }
        }

        // Allocate job slot but don't queue it yet
        auto handle = m_JobStorage.Allocate(std::move(job), signalSP, name, priority);
        if (!handle.IsValid())
        {
            VA_ENGINE_ERROR("[JobScheduler] Failed to allocate job slot for '{}'.", name);
            return InvalidJobHandle;
        }

        // Add job as continuation to dependency SyncPoint
        auto* depSyncPoint = m_SyncPointStorage.Get(dependency);
        if (!depSyncPoint)
        {
            VA_ENGINE_ERROR(
                "[JobScheduler] Invalid dependency sync point for job '{}'.",
                name ? name : "Unnamed");

            m_JobStorage.Release(handle);
            return InvalidJobHandle;
        }

        auto spname = depSyncPoint->GetDebugName();
        depSyncPoint->AddContinuation(handle);

        m_Stats.jobsSubmitted.fetch_add(1, std::memory_order_relaxed);

        VA_ENGINE_TRACE(
            "[JobScheduler] Submitted dependent job '{}' (handle: {}), waiting on sync point {}, with priority {}.",
            name,
            handle.GetPacked(),
            spname,
            GetPriorityString(priority));

        return handle;
    }

    // === Synchronization ===

    void JobScheduler::WaitFor(SyncPointHandle sp)
    {
        if (!sp.IsValid())
        {
            VA_ENGINE_WARN("[JobScheduler] WaitFor called with invalid sync point handle.");
            return;
        }

        // Help while waiting strategy - execute jobs from shared queues while waiting
        while (!IsSignaled(sp))
        {
            if (m_Shutdown.load(std::memory_order_acquire))
            {
                VA_ENGINE_WARN("[JobScheduler] WaitFor interrupted by shutdown.");
                break;
            }

            // Try to execute a job from shared priority queues while waiting
            auto jobHandle = PullJobFromQueues(UINT32_MAX); // Use Max as main thread
            if (jobHandle.IsValid())
            {
                ExecuteJob(jobHandle, UINT32_MAX);
            }
            else
            {
                // No jobs available, wait for a signal
                Platform::Thread::Yield();
            }
        }
    }

    size_t JobScheduler::WaitForMultiple(std::span<SyncPointHandle> syncPoints, bool waitForAll)
    {
        if (syncPoints.empty())
        {
            return SIZE_MAX;
        }

        while (true)
        {
            if (m_Shutdown.load(std::memory_order_acquire))
            {
                VA_ENGINE_WARN("[JobScheduler] WaitForMultiple interrupted by shutdown.");
                break;
            }

            // Check SyncPoints
            size_t signaledCount = 0;
            size_t firstSignaled = SIZE_MAX;

            for (size_t i = 0; i < syncPoints.size(); ++i)
            {
                if (IsSignaled(syncPoints[i]))
                {
                    signaledCount++;
                    if (firstSignaled == SIZE_MAX)
                    {
                        firstSignaled = i;
                    }
                }
            }

            // Check exit conditions
            if (waitForAll && signaledCount == syncPoints.size())
            {
                return SIZE_MAX;
            }
            else if (!waitForAll && firstSignaled != SIZE_MAX)
            {
                return firstSignaled;
            }

            // Help while waiting - execute jobs from shared queues
            auto jobHandle = PullJobFromQueues(UINT32_MAX);
            if (jobHandle.IsValid())
            {
                ExecuteJob(jobHandle, UINT32_MAX);
            }
            else
            {
                // No jobs available, wait for a signal
                Platform::Thread::Yield();
            }
        }

        return SIZE_MAX;
    }

    // === Statistics and Monitoring ===

    float JobScheduler::GetBackpressureLevel() const
    {
        size_t usedJobs = m_JobStorage.GetUsedSlots();
        size_t usedSyncPoints = m_SyncPointStorage.GetUsedSlots();

        auto jobPressure = static_cast<float>(usedJobs) / static_cast<float>(MAX_JOBS);
        float syncPressure = static_cast<float>(usedSyncPoints) / static_cast<float>(
            MAX_SYNCPOINTS);

        return std::max(jobPressure, syncPressure);
    }

    std::array<size_t, 4> JobScheduler::GetQueueLengths() const
    {
        std::array<size_t, 4> lengths{};
        for (size_t i = 0; i < 4; ++i)
        {
            if (m_PriorityQueues[i])
            {
                lengths[i] = m_PriorityQueues[i]->size_approx();
            }
        }

        return lengths;
    }

    // === Private Implementation ===

    void JobScheduler::WorkerThreadMain(uint32_t workerIndex)
    {
        Platform::Thread::SetCurrentThreadName(
            ("JobWorker_" + std::to_string(workerIndex)).c_str());

        m_ActiveWorkers.fetch_add(1, std::memory_order_relaxed);

        VA_ENGINE_DEBUG("[JobScheduler] Worker thread {} started.", workerIndex);

        while (!m_Shutdown.load(std::memory_order_acquire))
        {
            // Try to pull a job from priority queues
            auto jobHandle = PullJobFromQueues(workerIndex);

            if (jobHandle.IsValid())
            {
                ExecuteJob(jobHandle, workerIndex);
            }
            else
            {
                // No work available - yield briefly to avoid busy waiting
                Platform::Thread::Yield();
            }
        }

        m_ActiveWorkers.fetch_sub(1, std::memory_order_relaxed);

        VA_ENGINE_DEBUG("[JobScheduler] Worker thread {} stopped.", workerIndex);
    }

    JobHandle JobScheduler::PullJobFromQueues(uint32_t workerIndex)
    {
        // Use thread-local random for randomized starting point
        static thread_local std::random_device rd;
        static thread_local std::mt19937 gen(rd());
        static thread_local uint8_t pullOffset = gen() % PullWeights::Total;

        // Weighted pull array: [Criticalx8, Highx4, Normalx2, Lowx1]
        static constexpr std::array<JobPriority, PullWeights::Total> pullWeights = {
            // Critical (8 entries)
            JobPriority::Critical,
            JobPriority::Critical,
            JobPriority::Critical,
            JobPriority::Critical,
            JobPriority::Critical,
            JobPriority::Critical,
            JobPriority::Critical,
            JobPriority::Critical,
            // High (4 entries)
            JobPriority::High,
            JobPriority::High,
            JobPriority::High,
            JobPriority::High,
            // Normal (2 entries)
            JobPriority::Normal,
            JobPriority::Normal,
            // Low (1 entry)
            JobPriority::Low
        };

        // Try pulls based on weighted strategy
        for (uint8_t attempt = 0; attempt < PullWeights::Total; ++attempt)
        {
            auto index = (pullOffset + attempt) % PullWeights::Total;
            auto priority = pullWeights[index];
            auto priorityIndex = static_cast<uint8_t>(priority);

            JobHandle job;
            if (m_PriorityQueues[priorityIndex] && m_PriorityQueues[priorityIndex]->
                try_dequeue(job))
            {
                m_Stats.jobsInQueue.fetch_sub(1, std::memory_order_relaxed);
                return job;
            }
        }

        return InvalidJobHandle; // No jobs found
    }

    void JobScheduler::ExecuteJob(JobHandle jobHandle, uint32_t workerIndex)
    {
        auto job = m_JobStorage.Get(jobHandle);
        if (!job)
        {
            VA_ENGINE_WARN(
                "[JobScheduler] ExecuteJob called with invalid job handle on worker {}.",
                workerIndex);
            return;
        }

        m_Stats.jobsExecuting.fetch_add(1, std::memory_order_relaxed);

        // Mark execution start
        job->MarkExecutionStart();

        JobResult result;
        try
        {
            // Execute the job function
            result = job->executeFunction();
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_ERROR("[JobScheduler] Job '{}' failed: {}", job->debugName, e.what());
            result = JobResult::Failed("Exception during execution");
            m_Stats.jobsFailed.fetch_add(1, std::memory_order_relaxed);
        }
        catch (...)
        {
            VA_ENGINE_ERROR("[JobScheduler] Job '{}' failed: Unknown exception", job->debugName);
            result = JobResult::Failed("Unknown exception during execution");
            m_Stats.jobsFailed.fetch_add(1, std::memory_order_relaxed);
        }

        // Mark execution end and update stats
        job->MarkExecutionEnd();
        job->result = result;

        if (result.IsSuccess())
        {
            m_Stats.jobsCompleted.fetch_add(1, std::memory_order_relaxed);
        }
        else if (result.IsCancelled())
        {
            m_Stats.jobsCancelled.fetch_add(1, std::memory_order_relaxed);
        }

        m_Stats.jobsExecuting.fetch_sub(1, std::memory_order_relaxed);

        // Signal completion to SyncPoint
        if (job->signalOnCompletion.IsValid())
        {
            Signal(job->signalOnCompletion, result);
        }

        // Release job slot
        m_JobStorage.Release(jobHandle);
    }

    void JobScheduler::ProcessSyncPointCompletion(SyncPoint* sp)
    {
        if (!sp) return;

        m_Stats.syncPointsSignaled.fetch_add(1, std::memory_order_relaxed);

        auto finalStatus = sp->GetStatus();
        auto continuations = sp->GetContinuations();

        if (finalStatus == JobResult::Status::Success)
        {
            // Success - Activate all continuations
            for (auto continuation : continuations)
            {
                auto job = m_JobStorage.Get(continuation);
                if (job)
                {
                    EnqueueJob(continuation, job->priority);
                }
            }
        }
        else
        {
            // Failure / Cancellation - cancel all continuations
            auto reason = (finalStatus == JobResult::Status::Failed)
                ? "Dependency failed"
                : "Dependency cancelled";

            for (auto continuation : continuations)
            {
                auto job = m_JobStorage.Get(continuation);
                if (job)
                {
                    VA_ENGINE_DEBUG(
                        "[JobScheduler] Cancelling continuation job '{}' : {}",
                        job->debugName,
                        reason);

                    // Signal the job's sync point with cancellation
                    if (job->signalOnCompletion.IsValid())
                    {
                        Signal(job->signalOnCompletion, JobResult::Cancelled(reason));
                    }

                    // Release the job
                    m_JobStorage.Release(continuation);
                    m_Stats.jobsCancelled.fetch_add(1, std::memory_order_relaxed);
                }
            }
        }
    }

    void JobScheduler::EnqueueJob(JobHandle jobHandle, JobPriority priority)
    {
        auto priorityIndex = static_cast<uint8_t>(priority);
        if (priorityIndex >= 4)
        {
            VA_ENGINE_ERROR(
                "[JobScheduler] Invalid priority index: {}",
                static_cast<uint32_t>(priority));
            return;
        }

        if (m_PriorityQueues[priorityIndex] && m_PriorityQueues[priorityIndex]->try_enqueue(
            jobHandle))
        {
            m_Stats.jobsInQueue.fetch_add(1, std::memory_order_relaxed);
        }
        else
        {
            VA_ENGINE_ERROR(
                "[JobScheduler] Failed to enqueue job to priority queue {}.",
                GetPriorityString(priority));
        }
    }

    SubmissionResult JobScheduler::CheckBackpressure() const
    {
        static constexpr float WARNING_THRESHOLD = 0.8f;
        static constexpr float CRITICAL_THRESHOLD = 0.95f;

        float backpressure = GetBackpressureLevel();

        if (backpressure >= CRITICAL_THRESHOLD)
        {
            return SubmissionResult::StorageFull_Critical;
        }
        else if (backpressure >= WARNING_THRESHOLD)
        {
            return SubmissionResult::StorageFull_Retry;
        }
        else
        {
            return SubmissionResult::Success;
        }
    }

    void JobScheduler::InitializePriorityQueues()
    {
        try
        {
            for (size_t i = 0; i < 4; ++i)
            {
                m_PriorityQueues[i] = std::make_unique<moodycamel::ConcurrentQueue<JobHandle>>();
            }

            VA_ENGINE_DEBUG("[JobScheduler] Priority queues initialized successfully.");
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_ERROR("[JobScheduler] Failed to initialize priority queues: {}", e.what());
            throw std::runtime_error("Failed to initialize priority queues");
        }
    }

    void JobScheduler::InitializeWorkers(uint32_t workerCount)
    {
        try
        {
            m_Workers.reserve(workerCount);

            for (uint32_t i = 0; i < workerCount; ++i)
            {
                auto worker = Platform::ThreadFactory::CreateThread();
                if (!worker)
                {
                    throw std::runtime_error("Failed to create worker thread " + std::to_string(i));
                }

                auto workerName = "JobWorker_" + std::to_string(i);
                if (!worker->Start([this, i] { WorkerThreadMain(i); }, workerName.c_str()))
                {
                    throw std::runtime_error("Failed to start worker thread " + std::to_string(i));
                }

                m_Workers.push_back(std::move(worker));
            }

            VA_ENGINE_DEBUG("[JobScheduler] {} worker threads initialized.", workerCount);
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_ERROR("[JobScheduler] Failed to initialize workers: {}", e.what());
            throw std::runtime_error("Failed to initialize workers");
        }
    }

    void JobScheduler::CleanupResources()
    {
        // Clear worker threads
        m_Workers.clear();

        // Clear priority queues
        for (auto& queue : m_PriorityQueues)
        {
            queue.reset();
        }

        // NOTE: FixedStorage will automatically clean up remaining jobs and sync points in
        //  their destructors

        VA_ENGINE_DEBUG("[JobScheduler] Resources cleaned up");
    }
}
