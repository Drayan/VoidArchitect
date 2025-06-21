//
// Created by Michael Desmedt on 20/06/2025.
//

#include "JobSystem.hpp"

namespace VoidArchitect::Jobs
{
    // ==============================================================
    // === Global Instance Definition ===
    // ==============================================================

    std::unique_ptr<JobSystem> g_JobSystem = nullptr;

    // ==============================================================
    // === JobSystem Implementation ===
    // ==============================================================

    JobSystem::JobSystem(uint32_t workerCount)
    {
        VA_ENGINE_INFO("[JobSystem] Initializing with {} workers.", workerCount);

        try
        {
            m_Scheduler = std::make_unique<JobScheduler>(workerCount);
            VA_ENGINE_INFO("[JobSystem] Initialized successfully.");
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_ERROR("[JobSystem] Failed to initialize: {}", e.what());
            throw;
        }
    }

    JobSystem::~JobSystem()
    {
        if (m_Scheduler)
        {
            VA_ENGINE_INFO("[JobSystem] Shutting down...");

            try
            {
                m_Scheduler = nullptr;
                VA_ENGINE_INFO("[JobSystem] Shutdown successfully.");
            }
            catch (const std::exception& e)
            {
                VA_ENGINE_ERROR("[JobSystem] Failed to shutdown: {}", e.what());
                m_Scheduler.reset();
            }
        }
    }

    // ==============================================================
    // === Backend API Implementation ===
    // ==============================================================

    SyncPointHandle JobSystem::CreateSyncPoint(const uint32_t initialCount, const char* name) const
    {
        if (!m_Scheduler)
        {
            VA_ENGINE_ERROR("[JobSystem] CreateSyncPoint called but scheduler not initialized.");
            return InvalidSyncPointHandle;
        }

        return m_Scheduler->CreateSyncPoint(initialCount, name);
    }

    JobHandle JobSystem::Submit(
        JobFunction job,
        const SyncPointHandle signalSP,
        const JobPriority priority,
        const char* name) const
    {
        if (!m_Scheduler)
        {
            VA_ENGINE_ERROR("[JobSystem] Submit called but scheduler not initialized.");
            return InvalidJobHandle;
        }

        return m_Scheduler->Submit(std::move(job), signalSP, priority, name);
    }

    JobHandle JobSystem::SubmitAfter(
        const SyncPointHandle dependency,
        JobFunction job,
        const SyncPointHandle signalSP,
        const JobPriority priority,
        const char* name) const
    {
        if (!m_Scheduler)
        {
            VA_ENGINE_ERROR("[JobSystem] SubmitAfter called but scheduler not initialized.");
            return InvalidJobHandle;
        }

        return m_Scheduler->SubmitAfter(dependency, std::move(job), signalSP, priority, name);
    }

    void JobSystem::Signal(const SyncPointHandle sp, const JobResult result) const
    {
        if (!m_Scheduler)
        {
            VA_ENGINE_ERROR("[JobSystem] Signal called but scheduler not initialized.");
            return;
        }

        m_Scheduler->Signal(sp, result);
    }

    void JobSystem::Cancel(const SyncPointHandle sp, const char* reason) const
    {
        if (!m_Scheduler)
        {
            VA_ENGINE_ERROR("[JobSystem] Cancel called but scheduler not initialized.");
            return;
        }

        m_Scheduler->Cancel(sp, reason);
    }

    bool JobSystem::IsSignaled(const SyncPointHandle sp) const
    {
        if (!m_Scheduler)
        {
            VA_ENGINE_ERROR("[JobSystem] IsSignaled called but scheduler not initialized.");
            return false;
        }

        return m_Scheduler->IsSignaled(sp);
    }

    JobResult::Status JobSystem::GetSyncPointStatus(const SyncPointHandle sp) const
    {
        if (!m_Scheduler)
        {
            VA_ENGINE_ERROR("[JobSystem] GetSyncPointStatus called but scheduler not initialized.");
            return JobResult::Status::Failed;
        }

        return m_Scheduler->GetSyncPointStatus(sp);
    }

    void JobSystem::WaitFor(const SyncPointHandle sp) const
    {
        if (!m_Scheduler)
        {
            VA_ENGINE_ERROR("[JobSystem] WaitFor called but scheduler not initialized.");
            return;
        }

        m_Scheduler->WaitFor(sp);
    }

    // ==============================================================
    // === Frontend API Implementation ===
    // ==============================================================

    JobHandle JobSystem::SubmitJob(
        JobFunction job,
        const char* name,
        const JobPriority priority) const
    {
        if (!m_Scheduler)
        {
            VA_ENGINE_ERROR("[JobSystem] SubmitJob called but scheduler not initialized.");
            return InvalidJobHandle;
        }

        // Create a SyncPoint for this job
        auto syncPoint = CreateSyncPoint(1, name);
        if (!syncPoint.IsValid())
        {
            VA_ENGINE_ERROR("[JobSystem] Failed to create sync point for job '{}'.", name);
            return InvalidJobHandle;
        }

        // Submit the job
        return Submit(std::move(job), syncPoint, priority, name);
    }

    JobHandle JobSystem::SubmitJobAfter(
        JobHandle dependency,
        JobFunction job,
        const char* name,
        const JobPriority priority)
    {
        if (!m_Scheduler)
        {
            VA_ENGINE_ERROR("[JobSystem] SubmitJobAfter called but scheduler not initialized.");
            return InvalidJobHandle;
        }

        if (!dependency.IsValid())
        {
            VA_ENGINE_ERROR("[JobSystem] SubmitJobAfter called with invalid dependency.");
            return InvalidJobHandle;
        }

        // Get the dependency job's SyncPoint
        auto depJob = m_Scheduler->m_JobStorage.Get(dependency);
        if (!depJob)
        {
            VA_ENGINE_ERROR("[JobSystem] Dependency job not found for '{}'.", name);
            return InvalidJobHandle;
        }

        // Create SyncPoint for the new job
        auto syncPoint = CreateSyncPoint(1, name);
        if (!syncPoint.IsValid())
        {
            VA_ENGINE_ERROR("[JobSystem] Failed to create sync point for job '{}'.", name);
            return InvalidJobHandle;
        }

        // Submit after dependency's SyncPoint
        return SubmitAfter(depJob->signalOnCompletion, std::move(job), syncPoint, priority, name);
    }

    void JobSystem::WaitForJob(JobHandle handle)
    {
        if (!m_Scheduler)
        {
            VA_ENGINE_ERROR("[JobSystem] WaitForJob called but scheduler not initialized.");
            return;
        }

        if (!handle.IsValid())
        {
            VA_ENGINE_ERROR("[JobSystem] WaitForJob called with invalid handle.");
            return;
        }

        // Get the job's SyncPoint
        auto job = m_Scheduler->m_JobStorage.Get(handle);
        if (!job)
        {
            VA_ENGINE_ERROR("[JobSystem] Job not found for '{}'.", handle.GetPacked());
            return;
        }

        // Wait for the job's SyncPoint
        WaitFor(job->signalOnCompletion);
    }

    bool JobSystem::IsJobCompleted(JobHandle handle) const
    {
        if (!m_Scheduler)
        {
            VA_ENGINE_ERROR("[JobSystem] IsJobCompleted called but scheduler not initialized.");
            return true; // Consider it completed if system not running
        }

        if (!handle.IsValid())
        {
            VA_ENGINE_ERROR("[JobSystem] IsJobCompleted called with invalid handle.");
            return true; //Invalid jobs are considered completed
        }

        auto job = m_Scheduler->m_JobStorage.Get(handle);
        if (!job)
        {
            return true; // Job not found - probably completed and released
        }

        // Check if the job's SyncPoint is signaled
        return IsSignaled(job->signalOnCompletion);
    }

    JobResult JobSystem::GetJobResult(JobHandle handle) const
    {
        if (!m_Scheduler)
        {
            VA_ENGINE_ERROR("[JobSystem] GetJobResult called but scheduler not initialized.");
            return JobResult::Failed("Job system not initialized");
        }

        if (!handle.IsValid())
        {
            VA_ENGINE_ERROR("[JobSystem] GetJobResult called with invalid handle.");
            return JobResult::Failed("Invalid job handle");
        }

        auto job = m_Scheduler->m_JobStorage.Get(handle);
        if (!job)
        {
            // Job isn't found - it may have completed and been released
            // We can't know the result, so assume it was successful
            return JobResult::Success();
        }

        // If job is still in storage but SyncPoint is signalled, return the stored result
        if (IsSignaled(job->signalOnCompletion))
        {
            return job->result;
        }
        else
        {
            // Job is still running, return the pending result
            return JobResult::Failed("Job not yet completed");
        }
    }

    // ==============================================================
    // === Stats and Monitoring ===
    // ==============================================================

    const JobSystemStats& JobSystem::GetStats() const
    {
        if (!m_Scheduler)
        {
            // Return static empty stats if scheduler not available
            static JobSystemStats emptyStats;
            return emptyStats;
        }

        return m_Scheduler->GetStats();
    }

    float JobSystem::GetBackpressureLevel() const
    {
        if (!m_Scheduler)
        {
            return 0.0f;
        }

        return m_Scheduler->GetBackpressureLevel();
    }

    std::array<size_t, 4> JobSystem::GetQueueLengths() const
    {
        if (!m_Scheduler)
        {
            return {0, 0, 0, 0};
        }

        return m_Scheduler->GetQueueLengths();
    }

    bool JobSystem::IsRunning() const
    {
        return m_Scheduler && m_Scheduler->IsRunning();
    }

    // ==============================================================
    // === JobBatch Implementation ===
    // =============================================================

    JobSystem::JobBatch::JobBatch(const char* name)
        : m_BatchName(name ? name : "UnnamedBatch"),
          m_Submitted(false)
    {
        VA_ENGINE_TRACE("[JobBatch] Create batch '{}’.", m_BatchName);
    }

    void JobSystem::JobBatch::AddJob(JobFunction job, const char* name, JobPriority priority)
    {
        if (m_Submitted)
        {
            VA_ENGINE_ERROR(
                "[JobBatch] Cannot add job '{}' - batch '{}' already submitted.",
                name,
                m_BatchName);
            return;
        }

        if (!job)
        {
            VA_ENGINE_ERROR("[JobBatch] Cannot add job '{}' - job function is null.", name);
            return;
        }

        PendingJob pendingJob;
        pendingJob.function = std::move(job);
        pendingJob.name = name;
        pendingJob.priority = priority;

        m_PendingJobs.push_back(std::move(pendingJob));

        VA_ENGINE_TRACE(
            "[JobBatch] Added job '{}' to batch '{}' (total: {}).",
            name,
            m_BatchName,
            m_PendingJobs.size());
    }

    SyncPointHandle JobSystem::JobBatch::Submit()
    {
        if (m_Submitted)
        {
            VA_ENGINE_ERROR(
                "[JobBatch] Cannot submit batch '{}' - already submitted.",
                m_BatchName);
            return InvalidSyncPointHandle;
        }

        if (!g_JobSystem)
        {
            VA_ENGINE_ERROR(
                "[JobBatch] Cannot submit batch '{}' - job system not initialized.",
                m_BatchName);
            return InvalidSyncPointHandle;
        }

        if (m_PendingJobs.empty())
        {
            VA_ENGINE_WARN("[JobBatch] Submitting empty batch '{}'.", m_BatchName);

            // Create a SyncPoint that's already signaled for empty batches.
            m_BatchSyncPoint = g_JobSystem->CreateSyncPoint(1, m_BatchName);
            m_Submitted = true;
            return m_BatchSyncPoint;
        }

        // Create SyncPoint for the batch
        m_BatchSyncPoint = g_JobSystem->CreateSyncPoint(m_PendingJobs.size(), m_BatchName);
        if (!m_BatchSyncPoint.IsValid())
        {
            VA_ENGINE_ERROR("[JobBatch] Failed to create sync point for batch '{}'.", m_BatchName);
            return InvalidSyncPointHandle;
        }

        VA_ENGINE_DEBUG(
            "[JobBatch] Submitting batch '{}' with {} jobs.",
            m_BatchName,
            m_PendingJobs.size());

        // Submit all jobs to signal the batch SyncPoint
        size_t successCount = 0;
        for (const auto& pendingJob : m_PendingJobs)
        {
            auto jobHandle = g_JobSystem->Submit(
                pendingJob.function,
                m_BatchSyncPoint,
                pendingJob.priority,
                pendingJob.name);

            if (jobHandle.IsValid())
            {
                successCount++;
            }
            else
            {
                VA_ENGINE_ERROR(
                    "[JobBatch] Failed to submit job '{}' to batch '{}'.",
                    pendingJob.name,
                    m_BatchName);
            }
        }

        m_Submitted = true;

        if (successCount != m_PendingJobs.size())
        {
            VA_ENGINE_WARN(
                "[JobBatch] Only {}/{} jobs submitted successfully for batch '{}'.",
                successCount,
                m_PendingJobs.size(),
                m_BatchName);

            // Adjust SyncPoint counter to match actual submitted jobs
            auto syncPoint = g_JobSystem->m_Scheduler->m_SyncPointStorage.Get(m_BatchSyncPoint);
            if (syncPoint)
            {
                auto actualCount = static_cast<uint32_t>(successCount);
                syncPoint->counter.store(actualCount, std::memory_order_release);
            }
        }

        // Clear pending jobs since they're submitted
        m_PendingJobs.clear();

        return m_BatchSyncPoint;
    }

    void JobSystem::JobBatch::Cancel(const char* reason)
    {
        if (!m_Submitted)
        {
            VA_ENGINE_WARN(
                "[JobBatch] Cancelling unsubmitted batch '{}' - clearing {} pending jobs.",
                m_BatchName,
                m_PendingJobs.size());

            m_PendingJobs.clear();
            m_Submitted = true; // Prevent further submissions
            return;
        }

        if (!g_JobSystem || !m_BatchSyncPoint.IsValid())
        {
            VA_ENGINE_ERROR("[JobBatch] Cannot cancel batch '{}' - invalid state.", m_BatchName);
            return;
        }

        VA_ENGINE_INFO(
            "[JobBatch] Cancelling batch '{}' : {} - {} pending jobs.",
            m_BatchName,
            reason ? reason : "No reason specified",
            m_PendingJobs.size());

        // Cancel the batch SyncPoint - this will cascade to all batch jobs
        g_JobSystem->Cancel(m_BatchSyncPoint, reason);
    }
}
