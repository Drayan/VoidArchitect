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
        const char* name,
        uint32_t workerAffinity)
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

        Job jobObject(std::move(job), signalSP, name, priority, workerAffinity);

        // Allocate job slot
        auto handle = AllocateJobSlot(std::move(jobObject));
        if (!handle.IsValid())
        {
            VA_ENGINE_ERROR("[JobScheduler] Failed to allocate job slot for '{}'.", name);
            return InvalidJobHandle;
        }

        // Enqueue job for execution
        EnqueueJob(handle, priority);

        m_Stats.jobsSubmitted.fetch_add(1, std::memory_order_relaxed);

        const char* affinityStr = (workerAffinity == ANY_WORKER)
            ? "any"
            : (workerAffinity == MAIN_THREAD_ONLY)
            ? "main"
            : std::to_string(workerAffinity).c_str();

        VA_ENGINE_TRACE(
            "[JobScheduler] Submitted job '{}' (handle: {}) with priority {} (thread: {}).",
            name,
            handle.GetPacked(),
            GetPriorityString(priority),
            affinityStr);

        return handle;
    }

    JobHandle JobScheduler::SubmitAfter(
        SyncPointHandle dependency,
        JobFunction job,
        SyncPointHandle signalSP,
        JobPriority priority,
        const char* name,
        uint32_t workerAffinity)
    {
        // Check if the dependency is already signalled
        if (IsSignaled(dependency))
        {
            auto depStatus = GetSyncPointStatus(dependency);
            if (depStatus == JobResult::Status::Success)
            {
                // Job is already complete, submit job immediately
                return Submit(std::move(job), signalSP, priority, name, workerAffinity);
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

        Job jobObject(std::move(job), signalSP, name, priority, workerAffinity);

        // Allocate job slot but don't queue it yet
        auto handle = AllocateJobSlot(std::move(jobObject));
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

            UpdateJobStateCounts(JobState::Pending, static_cast<JobState>(-1));
            m_JobStorage.Release(handle);
            return InvalidJobHandle;
        }

        auto spname = depSyncPoint->GetDebugName();
        depSyncPoint->AddContinuation(handle);

        m_Stats.jobsSubmitted.fetch_add(1, std::memory_order_relaxed);

        const char* affinityStr = (workerAffinity == ANY_WORKER)
            ? "any"
            : (workerAffinity == MAIN_THREAD_ONLY)
            ? "main"
            : std::to_string(workerAffinity).c_str();

        VA_ENGINE_TRACE(
            "[JobScheduler] Submitted dependent job '{}' (handle: {}), waiting on sync point {}, with priority {} (thread: {}).",
            name,
            handle.GetPacked(),
            spname,
            GetPriorityString(priority),
            affinityStr);

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

    // === Main Thread Job Processing ===
    JobHandle JobScheduler::PullMainThreadJob()
    {
        // Use thread-local random for randomized starting point
        static thread_local std::random_device rd;
        static thread_local std::mt19937 gen(rd());
        static thread_local uint8_t pullOffset = gen() % PullWeights::Total;

        // Weighted pull array: [Criticalx8, Highx4, Normalx2, Lowx1]
        static constexpr std::array<JobPriority, PullWeights::Total> pullWeights = {
            JobPriority::Critical,
            JobPriority::Critical,
            JobPriority::Critical,
            JobPriority::Critical,
            JobPriority::Critical,
            JobPriority::Critical,
            JobPriority::Critical,
            JobPriority::Critical,
            JobPriority::High,
            JobPriority::High,
            JobPriority::High,
            JobPriority::High,
            JobPriority::Normal,
            JobPriority::Normal,
            JobPriority::Low
        };

        // Try pulls based on weighted strategy
        for (uint8_t attempt = 0; attempt < PullWeights::Total; ++attempt)
        {
            auto index = (pullOffset + attempt) % PullWeights::Total;
            auto priority = pullWeights[index];
            auto priorityIndex = static_cast<uint8_t>(priority);

            JobHandle job;
            if (m_MainThreadPriorityQueues[priorityIndex] && m_MainThreadPriorityQueues[
                priorityIndex]->try_dequeue(job))
            {
                m_Stats.jobsInQueue.fetch_sub(1, std::memory_order_relaxed);
                return job;
            }
        }

        return InvalidJobHandle; // No jobs found
    }

    bool JobScheduler::HasPendingMainThreadJobs() const
    {
        // Quick check across all priority queues
        for (uint8_t priorityIndex = 0; priorityIndex < 4; ++priorityIndex)
        {
            if (!m_MainThreadPriorityQueues[priorityIndex]) continue;

            // Check if this queue has any jobs
            if (m_MainThreadPriorityQueues[priorityIndex]->size_approx() > 0)
            {
                return true;
            }
        }

        return false;
    }

    bool JobScheduler::IsMainThreadJob(const Job* job)
    {
        return job && job->workerAffinity == MAIN_THREAD_ONLY;
    }

    // === Statistics and Monitoring ===

    float JobScheduler::GetBackpressureLevel() const
    {
        const auto activeJobs = m_JobStateCounts.activeJobs.load(std::memory_order_relaxed);
        const auto completedJobs = m_JobStateCounts.completedJobs.load(std::memory_order_relaxed);
        const auto completedN1Jobs = m_JobStateCounts.completedN1Jobs.load(
            std::memory_order_relaxed);
        const auto completedN2Jobs = m_JobStateCounts.completedN2Jobs.load(
            std::memory_order_relaxed);

        const auto maxJobsFloat = static_cast<float>(MAX_JOBS);

        // Layer 1: Excludee CompletedN2 jobs (immediately evictable)
        const auto layer1Count = activeJobs + completedJobs + completedN1Jobs;
        const auto layer1Pressure = static_cast<float>(layer1Count) / maxJobsFloat;
        if (layer1Pressure < 0.8f)
        {
            // Low pressure - system is healty, N2 jobs provide buffer
            return layer1Pressure;
        }

        // Layer 2: Exclude CompletedN1 jobs as well (evictable under pressure)
        const auto layer2Count = activeJobs + completedJobs;
        const auto layer2Pressure = static_cast<float>(layer2Count) / maxJobsFloat;
        if (layer2Pressure < 0.8f)
        {
            // Moderate pressure - N1 jobs will be evicted if needed
            return layer2Pressure;
        }

        // Layer 3: Only active jobs count (high/critical pressure)
        const auto layer3Pressure = static_cast<float>(activeJobs) / maxJobsFloat;
        return layer3Pressure;
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

        // Transition to Completed state
        auto oldState = job->state.exchange(JobState::Completed, std::memory_order_acq_rel);
        UpdateJobStateCounts(oldState, JobState::Completed);
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

        // Route job to appropriate queue based on affinity
        auto* job = m_JobStorage.Get(jobHandle);
        bool isMainThread = IsMainThreadJob(job);

        auto& targetQueue = !isMainThread
            ? m_PriorityQueues[priorityIndex]
            : m_MainThreadPriorityQueues[priorityIndex];

        if (targetQueue && targetQueue->try_enqueue(jobHandle))
        {
            m_Stats.jobsInQueue.fetch_add(1, std::memory_order_relaxed);
        }
        else
        {
            VA_ENGINE_ERROR(
                "[JobScheduler] Failed to enqueue {} job to priority queue {}.",
                isMainThread ? "main thread" : "worker thread",
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
                m_MainThreadPriorityQueues[i] = std::make_unique<moodycamel::ConcurrentQueue<
                    JobHandle>>();
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

    float JobSystemStats::GetJobsPerSecond(float deltaTimeSeconds) const
    {
    }

    float JobSystemStats::GetUtilizationPercentage() const
    {
    }

    float JobSystemStats::GetEvictionRate() const
    {
    }

    void JobScheduler::UpdateJobStateCounts(JobState oldState, JobState newState)
    {
        // Decrement old state counter
        auto oldCategory = GetStateCounterCategory(oldState);
        switch (oldCategory)
        {
            case 0:
                m_JobStateCounts.activeJobs.fetch_sub(1, std::memory_order_relaxed);
                break;

            case 1:
                m_JobStateCounts.completedJobs.fetch_sub(1, std::memory_order_relaxed);
                break;

            case 2:
                m_JobStateCounts.completedN1Jobs.fetch_sub(1, std::memory_order_relaxed);
                break;

            case 3:
                m_JobStateCounts.completedN2Jobs.fetch_sub(1, std::memory_order_relaxed);
                break;

            default:
                break;
        }

        // Increment new state counter
        auto newCategory = GetStateCounterCategory(newState);
        switch (newCategory)
        {
            case 0:
                m_JobStateCounts.activeJobs.fetch_add(1, std::memory_order_relaxed);
                break;

            case 1:
                m_JobStateCounts.completedJobs.fetch_add(1, std::memory_order_relaxed);
                break;

            case 2:
                m_JobStateCounts.completedN1Jobs.fetch_add(1, std::memory_order_relaxed);
                break;

            case 3:
                m_JobStateCounts.completedN2Jobs.fetch_add(1, std::memory_order_relaxed);
                break;

            default:
                break;
        }
    }

    int JobScheduler::GetStateCounterCategory(JobState state)
    {
        switch (state)
        {
            case JobState::Pending:
            case JobState::Cancelled:
            case JobState::Executing:
            case JobState::Ready:
                return 0;

            case JobState::Completed:
                return 1;

            case JobState::CompletedN1:
                return 2;

            case JobState::CompletedN2:
                return 3;

            default:
                return -1;
        }
    }

    void JobScheduler::PromoteCompletedJobs()
    {
        // Track promotion statistics for debugging / profiling
        size_t promotedToN1 = 0;
        size_t promotedToN2 = 0;

        // Iterate through all job storage slots
        // Note : This is safe to call from main thread as state transitions are atomic
        for (size_t slotIndex = 0; slotIndex < MAX_JOBS; ++slotIndex)
        {
            // Check if slot is in use (lock-free check)
            if (!m_JobStorage.IsUsed(slotIndex)) continue;

            auto& job = m_JobStorage[slotIndex];
            auto currentState = job.state.load(std::memory_order_acquire);

            switch (currentState)
            {
                case JobState::Completed:
                {
                    // Fresh completion -> Age by 1 frame
                    if (job.state.compare_exchange_weak(
                        currentState,
                        JobState::CompletedN1,
                        std::memory_order::acq_rel))
                    {
                        UpdateJobStateCounts(JobState::Completed, JobState::CompletedN1);
                        promotedToN1++;
                    }
                    break;
                }

                case JobState::CompletedN1:
                {
                    // One frame old -> age by 2 frames (now eligible for eviction)
                    if (job.state.compare_exchange_weak(
                        currentState,
                        JobState::CompletedN2,
                        std::memory_order::acq_rel))
                    {
                        UpdateJobStateCounts(JobState::CompletedN1, JobState::CompletedN2);
                        promotedToN2++;
                    }
                    break;
                }

                default:
                    break;
            }
        }

        // Log promotion activity for debug (only when promotion occurs)
        if (promotedToN1 > 0 || promotedToN2 > 0)
        {
            VA_ENGINE_TRACE(
                "[JobScheduler] Promoted {} jobs to N1 and {} jobs to N2.",
                promotedToN1,
                promotedToN2);
        }
    }

    JobHandle JobScheduler::AllocateJobSlot(Job&& job)
    {
        // Phase 1: Try normal allocation first
        auto handle = m_JobStorage.Allocate(std::move(job));
        if (handle.IsValid())
        {
            // Track the new job in our counters
            UpdateJobStateCounts(static_cast<JobState>(-1), JobState::Pending);
            return handle;
        }

        // Phase 2: Simple eviction - try oldest first
        // Step 2a: Try to evict CompletedN2 jobs
        if (TryEvictJobByState(JobState::CompletedN2))
        {
            handle = m_JobStorage.Allocate(std::move(job));
            if (handle.IsValid())
            {
                // Track the new job in our counters
                UpdateJobStateCounts(static_cast<JobState>(-1), JobState::Pending);
                return handle;
            }
        }

        // Step 2b: Try to evict CompletedN1 jobs
        if (TryEvictJobByState(JobState::CompletedN1))
        {
            handle = m_JobStorage.Allocate(std::move(job));
            if (handle.IsValid())
            {
                // Track the new job in our counters
                UpdateJobStateCounts(static_cast<JobState>(-1), JobState::Pending);
            }
        }

        // Step 2c: Last resort - evict fresh Completed jobs
        if (TryEvictJobByState(JobState::Completed))
        {
            handle = m_JobStorage.Allocate(std::move(job));
            if (handle.IsValid())
            {
                // Track the new job in our counters
                UpdateJobStateCounts(static_cast<JobState>(-1), JobState::Pending);
                return handle;
            }
        }

        // Complete failure - log useful diagnostics
        VA_ENGINE_CRITICAL(
            "[JobScheduler] Critical allocation failure - Active: {}, Completed: {}, N1: {}, N2: {}.",
            m_JobStateCounts.activeJobs.load(),
            m_JobStateCounts.completedJobs.load(),
            m_JobStateCounts.completedN1Jobs.load(),
            m_JobStateCounts.completedN2Jobs.load());

        return InvalidJobHandle;
    }

    bool JobScheduler::TryEvictJobByState(JobState targetState)
    {
        // Search through storage to find a job in the target state
        for (size_t slotIndex = 0; slotIndex < m_JobStorage.GetCapacity(); ++slotIndex)
        {
            // Check if slot is in use (lock-free check)
            if (!m_JobStorage.IsUsed(slotIndex)) continue;

            auto& job = m_JobStorage[slotIndex];
            auto currentState = job.state.load(std::memory_order_acquire);
            if (currentState == targetState)
            {
                // Found a candidate - get a valid handle for this slot
                auto evictHandle = m_JobStorage.GetHandleForSlot(slotIndex);

                if (evictHandle.IsValid())
                {
                    // Track the eviction
                    UpdateJobStateCounts(targetState, static_cast<JobState>(-1));

                    // Update eviction statistics based on state
                    switch (targetState)
                    {
                        case JobState::CompletedN2:
                            m_Stats.jobsEvictedN2.fetch_add(1, std::memory_order_relaxed);
                            break;
                        case JobState::CompletedN1:
                            m_Stats.jobsEvictedN1.fetch_add(1, std::memory_order_relaxed);
                            break;
                        case JobState::Completed:
                            m_Stats.jobsEvictedCompleted.fetch_add(1, std::memory_order_relaxed);
                            break;
                        default:
                            break;
                    }

                    // Perform the actual eviction
                    auto released = m_JobStorage.Release(evictHandle);
                    if (released)
                    {
                        return true;
                    }
                    else
                    {
                        // Eviction failed - restore counter
                        UpdateJobStateCounts(static_cast<JobState>(-1), targetState);
                        VA_ENGINE_WARN(
                            "[JobScheduler] Failed to release job handle for slot {}.",
                            slotIndex);
                    }
                }
            }
        }

        return false;
    }
}
