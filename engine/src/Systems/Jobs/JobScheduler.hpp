//
// Created by Michael Desmedt on 20/06/2025.
//
#pragma once

#include "JobTypes.hpp"
#include "SyncPoint.hpp"
#include "Core/Collections/FixedStorage.hpp"

namespace moodycamel
{
    template <typename T>
    class ConcurrentQueue;
}

namespace VoidArchitect::Jobs
{
    // === Forward Declaration ===
    class JobSystem;

    // === Job System Statistics ===

    /// @brief Statistics structure for job system performance monitoring
    ///
    /// Provides comprehensive metrics for profiling and debugging the job system.
    /// All counters are atomic for thread-safe access from multiple contexts.
    struct JobSystemStats
    {
        /// @brief Total number of jobs submitted since initialization
        std::atomic<uint64_t> jobsSubmitted{0};

        /// @brief Total number of jobs completed successfully
        std::atomic<uint64_t> jobsCompleted{0};

        /// @brief Total number of jobs that failed during execution
        std::atomic<uint64_t> jobsFailed{0};

        /// @brief Total number of jobs cancelled before execution
        std::atomic<uint64_t> jobsCancelled{0};

        /// @brief Total number of SyncPoints created
        std::atomic<uint64_t> syncPointsCreated{0};

        /// @brief Total number of SyncPoints signaled
        std::atomic<uint64_t> syncPointsSignaled{0};

        /// @brief Number of jobs currently in queues
        std::atomic<uint64_t> jobsInQueue{0};

        /// @brief Number of jobs currently executing
        std::atomic<uint64_t> jobsExecuting{0};

        /// @brief Number of active worker threads
        std::atomic<uint32_t> activeWorkers{0};

        /// @brief Get jobs per second throughput
        /// @param deltaTimeSeconds Time delta for calculation
        /// @return Estimated jobs per second
        float GetJobsPerSecond(float deltaTimeSeconds) const;

        /// @brief Get current job system utilization percentage
        /// @return Utilization as percentage (0.0 to 100.0)
        float GetUtilizationPercentage() const;
    };

    // === Submission Result ===

    /// @brief Result of job submission operations
    ///
    /// Provides information about whether job submission succeeded and
    /// indicates backpressure conditions for adaptive behaviour.
    enum class SubmissionResult : uint8_t
    {
        Success = 0, ///< Job submitted successfully
        StorageFull_Retry, ///< Storage is full but retry is possible
        StorageFull_Critical ///< Storage critically full, emergency action are needed.
    };

    // === JobScheduler Core Class ===

    /// @brief Central job scheduling and execution system
    ///
    /// JobScheduler is the core of VoidArchitect's multithreading system, providing
    /// efficient job submission, dependency management, and work distribution across
    /// multiple worker threads.
    ///
    /// # Key architectural features:
    /// - Lock-free job submission and priority queues
    /// - SyncPoint-based dependency management with automatic cascading
    /// - Work stealing for load balancing
    /// - Backpressure handling for system stability
    /// - Comprehensive statistics and profiling support
    ///
    /// # Priority Scheduling:
    /// - Uses shared priority queues accessible by all workers
    /// - Uses weighted pull strategy to prevent starvation
    /// - Critical: 8/15 pulls, High: 4/15 pulls, Normal: 2/15 pulls, Low: 1/15 pulls
    ///
    /// # Memory Management:
    /// - Fixed-capacity storage using FixedStorage pattern
    /// - Configurable limits with backpressure detection
    /// - Automatic cleanup of completed jobs and SyncPoints
    ///
    /// # Thread Safety:
    /// - All public methods are thread-safe
    /// - Lock-free hot paths for job submission and completion
    ///
    /// # Usage example:
    /// @code
    /// // Initialize with auto-detected worker count
    /// auto scheduler = std::make_unique<JobScheduler>();
    ///
    /// // Create sync point for coordinating multiple jobs
    /// auto chunksSP = scheduler->CreateSyncPoint(4, "ChunkGeneration");
    ///
    /// // Submit jobs that signal the sync point
    /// for(int i = 0; i < 4; ++i)
    /// {
    ///     scheduler->Submit([i]() -> JobResult
    ///     {
    ///         GenerateChunk(i);
    ///         return JobResult::Success();
    ///     }, chunkSP, JobPriority::Normal, "GenerateChunk");
    /// }
    ///
    /// // Submit follow-up job that depends on sync point
    /// auto renderSP = scheduler->CreateSyncPoint(1, "RenderComplete")
    /// scheduler->SubmitAfter(chunksSP, []() -> JobResult
    /// {
    ///     RenderChunks();
    ///     return JobResult::Success();
    /// }, renderSP, JobPriority::Normal, "RenderChunks");
    ///
    /// // Wait for completion
    /// scheduler->WaitFor(renderSP);
    /// @encode
    class JobScheduler
    {
    public:
        // === Lifecycle Management ===

        /// @brief Initialize the JobScheduler with a specified worker count
        /// @param workerCount Number of worker threads (0 = auto-detect)
        ///
        /// Automatically initialize priority queues and worker threads.
        /// @throws std::runtime_error if initialization fails
        explicit JobScheduler(uint32_t workerCount = 0);

        /// @brief Destructor handles graceful shutdown
        ///
        /// Automatically signals workers to stop, waits for job completion,
        /// joins worker threads, and cleans up all allocated resources.
        ~JobScheduler();

        // Non-copyable and non-movable
        JobScheduler(const JobScheduler&) = delete;
        JobScheduler& operator=(const JobScheduler&) = delete;
        JobScheduler(JobScheduler&&) = delete;
        JobScheduler& operator=(JobScheduler&&) = delete;

        /// @brief Check if job system if running
        /// @return true if system is operational (always true for constructed objects)
        bool IsRunning() const
        {
            return !m_Shutdown.load(std::memory_order_acquire);
        }

        // === SyncPoint Management ===

        /// @brief Create a new SyncPoint for dependency coordination
        /// @param initialCount Number of dependencies that must complete before signalling
        /// @param name Debug name for the SyncPoint (should be static string)
        /// @return Handle to the created SyncPoint, or invalid handle if creation failed.
        ///
        /// SyncPoints are the core mechanism for Job dependencies. They provide:
        /// - Atomic counter-based coordination
        /// - Status aggregation (Success/ Failure / Cancellation)
        /// - Automatic continuation activation when signaled
        /// - Debug information for profiling
        SyncPointHandle CreateSyncPoint(uint32_t initialCount, const char* name);

        /// @brief Manually signal a SyncPoint (decrements counter)
        /// @param sp Handle to SyncPoint to signal
        /// @param result Result to propagate to the SyncPoint
        ///
        /// This method is used for manual SyncPoint coordination or when
        /// integrating with external systems that don't use the job system.
        void Signal(SyncPointHandle sp, JobResult result);

        /// @brief Cancel a SyncPoint and all its continuations
        /// @param sp Handle to SyncPoint to cancel
        /// @param reason Description of why cancellation occurred
        ///
        /// Cancellation propagates through the dependency graph, automatically
        /// cancelling any jobs that depend on the cancelled SyncPoint.
        void Cancel(SyncPointHandle sp, const char* reason);

        /// @brief Check if SyncPoint is signalled (counter reached zero)
        /// @param sp Handle to SyncPoint to check
        /// @return true if SyncPoint is signalled, false otherwise or if invalid handle
        bool IsSignaled(SyncPointHandle sp) const;

        /// @brief Get current status of SyncPoint
        /// @param sp Handle to SyncPoint to query
        /// @return Current status, or Failed if invalid handle
        JobResult::Status GetSyncPointStatus(SyncPointHandle sp) const;

        // === Job Submission (Backend API) ===

        /// @brief Submit a job for execution
        /// @param job Function to execute
        /// @param signalSP SyncPoint to signal when job completes
        /// @param priority Job execution priority
        /// @param name Debug name for the job (should be static string)
        /// @return Handle to a submitted job, or invalid handle if submission failed
        ///
        /// This is the code job submission method. The job will be queued based on
        /// priority and executed when a worker becomes available. Upon completion,
        /// the specified SyncPoint will be signalled with the job's result.
        JobHandle Submit(
            JobFunction job,
            SyncPointHandle signalSP,
            JobPriority priority,
            const char* name);

        /// @brief Submit a job to execute after a dependency is satisfied
        /// @param dependency SyncPoint that must be signalled before execution
        /// @param job Function to execute
        /// @param signalSP SyncPoint to signal when job completes
        /// @param priority Job execution priority
        /// @param name Debug name for the job
        /// @return Handle to a submitted job, or invalid handle if submission failed
        ///
        /// The job will be held in a pending state until the dependency SyncPoint
        /// is signalled. If the dependency fails or is cancelled, this jow will
        /// automatically be cancelled as well.
        JobHandle SubmitAfter(
            SyncPointHandle dependency,
            JobFunction job,
            SyncPointHandle signalSP,
            JobPriority priority,
            const char* name);

        // === Synchronization (Main Thread Only ===

        /// @brief Wait for a SyncPoint to be signalled (blocking)
        /// @param sp Handle to SyncPoint to wait for
        ///
        /// This method uses the "help while waiting" strategy - the calling thread
        /// will execute jobs from the queues while waiting, maximizing CPU utilization.
        ///
        /// @warning This method should only be called from the main thread or
        /// threads that are not part of the worker pool to avoid deadlocks.
        void WaitFor(SyncPointHandle sp);

        /// @brief Wait for multiple SyncPoints to be signalled
        /// @param syncPoints Array of SyncPoint handles to wait for
        /// @param waitForAll If true, wait for all SyncPoints; if false, wait for anyone
        /// @return Index of first signalled SyncPoint (if waitForAll=false), or SIZE_MAX
        size_t WaitForMultiple(std::span<SyncPointHandle> syncPoints, bool waitForAll = true);

        // === Statistics and Monitoring ===

        /// @brief Get current job system statistics
        /// @return Statistics structure with current metrics
        ///
        /// Statistics are updated atomically and can be safely accessed from any thread.
        /// Useful for performance monitoring, debugging, and adaptive behaviour.
        const JobSystemStats& GetStats() const { return m_Stats; }

        /// @brief Get the current backpressure level
        /// @return Value between 0.0 (empty) and 1.0 (full)
        ///
        /// Backpressure indicates how close the job system is to capacity.
        /// Values abode 0.8 indicate a high load, above 0.95 indicate a critical load.
        float GetBackpressureLevel() const;

        /// @brief Get the number of jobs in each priority queue
        /// @return Array of queue lengths [Critical, High, Normal, Low]
        std::array<size_t, 4> GetQueueLengths() const;

        friend class ::VoidArchitect::Jobs::JobSystem;

    private:
        // === Storage and Memory Management ===

        /// @brief Fixed storage for job objects
        FixedStorage<Job, MAX_JOBS> m_JobStorage;

        /// @brief Fixed storage for SyncPoint objects
        FixedStorage<SyncPoint, MAX_SYNCPOINTS> m_SyncPointStorage;

        // === Worker Thread Management ===

        /// @brief Worker thread instances
        VAArray<std::unique_ptr<Platform::IThread>> m_Workers;

        /// @brief Shutdown flag for coordinated worker termination
        std::atomic<bool> m_Shutdown{false};

        /// @brief Number of currently active workers
        std::atomic<uint32_t> m_ActiveWorkers{0};

        // === Priority Queue System ===

        /// @brief Priority-based job queues (lock-free)
        std::array<std::unique_ptr<moodycamel::ConcurrentQueue<JobHandle>>, 4> m_PriorityQueues;

        /// @brief Pull weights for anti-starvation scheduling
        struct PullWeights
        {
            static constexpr uint8_t Critical = 8;
            static constexpr uint8_t High = 4;
            static constexpr uint8_t Normal = 2;
            static constexpr uint8_t Low = 1;
            static constexpr uint8_t Total = Critical + High + Normal + Low;
        };

        // === Statistics ===

        /// @brief Performance and usage statistics
        mutable JobSystemStats m_Stats;

        // === Private Implementation Methods ===

        /// @brief Worker thread main loop
        /// @param workerIndex Index of this worker (for debugging/profiling)
        ///
        /// Each worker continuously:
        /// 1. Attempts to pull jobs from priority queues using a weighted strategy
        /// 2. Executes jobs and handles completion signaling
        /// 3. Updates statistics and handles errors
        /// 4. Yields when no work is available to avoid busy waiting
        void WorkerThreadMain(uint32_t workerIndex);

        /// @brief Pull a job from priority queues using a weighted strategy
        /// @param workerIndex Index of worker performing pull (for randomization)
        /// @return Job handle if found, invalid handle if all queues empty
        ///
        /// Implements anti-starvation algorithm with randomized starting points
        /// to prevent synchronization patterns between workers.
        JobHandle PullJobFromQueues(uint32_t workerIndex);

        /// @brief Execute a job and handle completion signalling
        /// @param jobHandle Handle to job to execute
        /// @param workerIndex Index of executing worker
        ///
        /// Safely executes a job function, catches exceptions, updates timing,
        /// and signals the associated SyncPoint with the result.
        void ExecuteJob(JobHandle jobHandle, uint32_t workerIndex);

        /// @brief Process SyncPoint signalling and continuation activation
        /// @param sp SyncPoint that was just signalled
        ///
        /// When a SyncPoint reaches zero counts, this method:
        /// 1. Determines if continuations should be activated or cancelled
        /// 2. Queues appropriate continuations based on final status
        /// 3. Updates statistics and logs debug information
        void ProcessSyncPointCompletion(SyncPoint* sp);

        /// @brief Enqueue a job into appropriate priority queue
        /// @param jobHandle Handle to job to enqueue
        /// @param priority Priority level for the job
        ///
        /// Updates queue statistics and handles any queue-full conditions
        void EnqueueJob(JobHandle jobHandle, JobPriority priority);

        /// @brief Check and handle backpressure conditions
        /// @return Current submission result based on system load
        ///
        /// Monitors system capacity and returns the appropriate submission result
        /// to enable adaptive behaviour in client systems.
        SubmissionResult CheckBackpressure() const;

        /// @brief Initialize priority queues
        /// @throws std::runtime_error if initialization fails
        void InitializePriorityQueues();

        /// @brief Initialize worker threads
        /// @param workerCount Number of workers to create
        /// @throws std::runtime_error if initialization fails
        void InitializeWorkers(uint32_t workerCount);

        /// @brief Clean up all resources during shutdown
        void CleanupResources();
    };
}
