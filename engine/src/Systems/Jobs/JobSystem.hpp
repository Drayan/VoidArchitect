//
// Created by Michael Desmedt on 20/06/2025.
//
#pragma once

#include "JobScheduler.hpp"

/// @file JobSystem.hpp
/// @brief JobSystem class following VoidArchitect engine patterns

namespace VoidArchitect::Jobs
{
    /// @brief Main job system class managing multithreaded job execution
    ///
    /// JobSystem provides the primary interface for job submission and dependency
    /// management throughout VoidArchitect. It manages an internal JobScheduler
    /// and provides both Backend (detailed control) and Frontend (simplified) APIs.
    ///
    /// Architecture follows VoidArchitect patterns:
    /// - Constructor performs all initialization automatically
    /// - Destructor handles graceful shutdown and cleanup
    /// - Global singleton access via g_JobSystem
    /// - Thread-safe operations for use accross all engine systems
    ///
    /// Integration with engine systems:
    /// - Rendering: Parallel culling, command generation, resource streaming
    /// - Physics: Board/narrow phase collision, constraint solving
    /// - Audio: Sample processing, 3D positioning, mixing
    /// - Networking: Packet processing, state synchronization
    /// - Assets: Asynchronous loading, texture streaming, mesh processing
    /// - Procedural: Chunk generation, terrain processing, content creation
    ///
    /// Usage example:
    /// @code
    /// // System initialization
    /// g_JobSystem = std::make_unique<JobSystem>();
    ///
    /// // Backend API - Full control with SyncPoints
    /// auto sp = g_JobSystem->CreateSyncPoint(2, "PreparationPhase");
    /// g_JobSystem->Submit(PrepareTextureJob, sp, JobPriority::Normal, "PrepareTextures");
    /// g_JobSystem->Submit(PrepareMeshesJob, sp, JobPriority::Normal, "PrepareMeshes");
    ///
    /// auto renderSP = g_JobSystem->CreateSyncPoint(1, "Rendering");
    /// g_JobSystem->SubmitAfter(sp, RenderJob, renderSP, JobPriority::Normal, "Render");
    ///
    /// g_JobSystem->WaitFor(renderSP);
    ///
    /// // Frontend API - Simplified interface
    /// auto job = g_JobSystem->SubmitJob(ProcessAudioJob, "AudioProcessing");
    /// g_JobSystem->WaitForJob(job);
    ///
    /// // Automatic cleanup when g_JobSystem go out of scope or = nullptr.
    /// @endcode
    class JobSystem
    {
    public:
        // === Lifecycle Management ===

        /// @brief Constructor initializes the job system
        /// @param workerCount Number of worker threads (0 = auto-detect)
        ///
        /// Automatically initializes the internal JobScheduler with the specified
        /// number of worker threads. If workerCount is 0, uses auto-detection
        /// based on hardware concurrency (cores - 1 to reserve main thread).
        ///
        /// @throws std::runtime_error if initialization fails
        explicit JobSystem(uint32_t workerCount = 0);

        /// @brief Destructor handles graceful shutdown
        ///
        /// Automatically shuts down all worker threads, waits for job completion,
        /// and cleans up all allocated resources. No manual shutdown required.
        ~JobSystem();

        // Non-copyable and non-movable
        JobSystem(const JobSystem&) = delete;
        JobSystem& operator=(const JobSystem&) = delete;
        JobSystem(JobSystem&&) = delete;
        JobSystem& operator=(JobSystem&&) = delete;

        // === Backend API (Full Control) ===

        /// @brief Create a new SyncPoint for dependency coordination
        /// @param initialCount Number of dependencies that must complete before signalling
        /// @param name Debug name for the SyncPoint (should be static string)
        /// @return Handle to the created SyncPoint, or invalid handle if creation failed
        SyncPointHandle CreateSyncPoint(uint32_t initialCount, const char* name) const;

        /// @brief Submit a job for execution with explicit SyncPoint
        /// @param job Function to execute
        /// @param signalSP SyncPoint to signal when job completes
        /// @param priority Job execution priority
        /// @param name Debug name for the job (should be static string)
        /// @return Handle to the submitted job, or invalid handle if submission failed
        JobHandle Submit(
            JobFunction job,
            SyncPointHandle signalSP,
            JobPriority priority,
            const char* name) const;

        /// @brief Submit a job that executes after another job completes
        /// @param dependency Job that must complete first
        /// @param job Function to execute
        /// @param signalSP Sync point to signal when a job completes
        /// @param name Debug name for the job
        /// @param priority Job execution priority
        /// @return Handle to the submitted job, or invalid handle if submission failed
        JobHandle SubmitAfter(
            SyncPointHandle dependency,
            JobFunction job,
            SyncPointHandle signalSP,
            JobPriority priority,
            const char* name) const;

        /// @brief Manually signal a SyncPoint
        /// @param sp Handle to SyncPoint to signal
        /// @param result Result to propagate to the SyncPoint
        void Signal(SyncPointHandle sp, JobResult result) const;

        /// @brief Cancel a SyncPoint and all its continuations
        /// @param sp Handle to SyncPoint to cancel
        /// @param reason Description of why cancellation occurred
        void Cancel(SyncPointHandle sp, const char* reason) const;

        /// @brief Check if SyncPoint is signaled
        /// @param sp Handle to SyncPoint to check
        /// @return true if SyncPoint is signaled
        bool IsSignaled(SyncPointHandle sp) const;

        /// @brief Get current status of SyncPoint
        /// @param sp Handle to SyncPoint to query
        /// @return Current status, or Failed if invalid handle
        JobResult::Status GetSyncPointStatus(SyncPointHandle sp) const;

        /// @brief Wait for a SyncPoint to be signaled (blocking)
        /// @param sp Handle to SyncPoint to wait for
        ///
        /// Uses "help while waiting" strategy - executing other jobs while waiting.
        /// Should only be called from main thread to avoid deadlocks.
        ///
        /// @warning This should only be called from main thread to avoid deadlocks.
        void WaitFor(SyncPointHandle sp) const;

        // === Frontend API ===

        /// @brief Submit a simple job (automatically creates SyncPoint)
        /// @param job Function to execute
        /// @param name Debug name for the job
        /// @param priority Job execution priority
        /// @return Handle to submitted job, or invalid handle if submission failed
        JobHandle SubmitJob(
            JobFunction job,
            const char* name,
            JobPriority priority = JobPriority::Normal) const;

        /// @brief Submit a job that executes after another job completes
        /// @param dependency Job that must complete first
        /// @param job Function to execute
        /// @param name Debug name for the job
        /// @param priority Job execution priority
        /// @return Handle to submitted job, or invalid handle if submission failed
        JobHandle SubmitJobAfter(
            JobHandle dependency,
            JobFunction job,
            const char* name,
            JobPriority priority = JobPriority::Normal);

        /// @brief Wait for a job to complete (blocking)
        /// @param handle Handle to job to wait for
        void WaitForJob(JobHandle handle);

        /// @brief Check if a job has completed
        /// @param handle Handle to job to check
        /// @return true if job has completed
        bool IsJobCompleted(JobHandle handle) const;

        /// @brief Get result of a completed job
        /// @param handle Handle to job to query
        /// @return Job result, or Failed result if job hasn't completed
        JobResult GetJobResult(JobHandle handle) const;

        // === Batch operations ===

        /// @brief Utility class for managing batches of related jobs
        class JobBatch
        {
        public:
            /// @brief Constructor creates empty job batch
            /// @param name Debug name for batch sync point
            explicit JobBatch(const char* name);

            ~JobBatch() = default;

            // Non-copyable but movable
            JobBatch(const JobBatch&) = delete;
            JobBatch& operator=(const JobBatch&) = delete;
            JobBatch(JobBatch&&) = default;
            JobBatch& operator=(JobBatch&&) = default;

            /// @brief Add a job to the batch
            /// @param job Function to execute
            /// @param name Debug name for the job
            /// @param priority Job execution priority
            void AddJob(
                JobFunction job,
                const char* name,
                JobPriority priority = JobPriority::Normal);

            /// @brief Submit all jobs in the batch for execution
            /// @return SyncPoint handle that completes when all batch jobs complete
            SyncPointHandle Submit();

            /// @brief Cancel all jobs in the batch
            /// @param reason Description of why batch was cancelled
            void Cancel(const char* reason = "Batch cancelled");

            /// @brief Get number of jobs in the batch
            /// @return Number of jobs added to the batch
            size_t GetJobCount() const { return m_PendingJobs.size(); }

            /// @brief Check fi batch has been submitted
            /// @return true if Submit() has been called
            bool IsSubmitted() const { return m_Submitted; }

        private:
            /// @brief Pending jobs to be submitted
            struct PendingJob
            {
                JobFunction function;
                const char* name;
                JobPriority priority;
            };

            VAArray<PendingJob> m_PendingJobs;
            SyncPointHandle m_BatchSyncPoint{InvalidSyncPointHandle};
            const char* m_BatchName;
            bool m_Submitted{false};
        };

        // === Statistics and Monitoring ===

        /// @brief Get current job system statistics
        /// @return Statistics structure with current metrics
        const JobSystemStats& GetStats() const;

        /// @brief Get current backpressure level
        /// @return Value between 0.0 (empty) and 1.0 (full)
        float GetBackpressureLevel() const;

        /// @brief Get number of jobs in each priority queue
        /// @return Array of queue lengths [Critical, High, Normal, Low]
        std::array<size_t, 4> GetQueueLengths() const;

        /// @brief Check if job system is running
        /// @return true if system is operational
        bool IsRunning() const;

        /// @brief Internal job scheduler managing all operations
        std::unique_ptr<JobScheduler> m_Scheduler;
    };

    // === Global Instance Declaration ===

    /// @brief Global job system instance (follows engine pattern)
    extern std::unique_ptr<JobSystem> g_JobSystem;
}
