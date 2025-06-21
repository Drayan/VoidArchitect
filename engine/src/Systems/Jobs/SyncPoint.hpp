//
// Created by Michael Desmedt on 20/06/2025.
//
#pragma once
#include "JobTypes.hpp"
#include "Core/Handle.hpp"

namespace VoidArchitect::Jobs
{
    // === Forward Declarations ===

    struct Job;
    struct SyncPoint;

    using JobHandle = Handle<Job>;
    using SyncPointHandle = Handle<SyncPoint>;

    // === SyncPoint Core Structure ===

    /// @brief Thread-safe synchronization point for job dependency management
    ///
    /// SyncPoint provides a robust mechanism for coordinating job execution dependencies
    /// in a lock-free manner. It supports atomic counter-based synchronization with
    /// status propagation and continuation management.
    ///
    /// # Key features:
    /// - Atomic dependency counting with status aggregation
    /// - Lock-free hot path for counter operations
    /// - Hybrid continuation storage (inline + overflow)
    /// - Automatic failure/cancellation cascade propagation
    /// - Debug-friendly naming and timing information
    ///
    /// # Architecture:
    /// - Hot data (counter, status) uses atomic operations for lock-free access
    /// - Continuations use hybrid storage : 6 inline slots + overflow vector
    /// - Status can only degrade (Success -> Failed/Cancelled) for safety
    /// - Thread-safe continuation management with minimal locking
    ///
    /// # Usage example:
    /// @code
    /// // Create sync point for fan-in dependency (wait for 3 jobs)
    /// SyncPoint syncPoint(3, "ChunkGenerationComplete");
    ///
    /// // Add continuation jobs
    /// syncPoint.AddContinuation(renderJobHandle);
    /// syncPoint.AddContinuation(collisionJobHandle);
    ///
    /// // Jobs signal completion (automatically decrements counter)
    /// syncPoint.DecrementAndCheck(JobResult::Success()); // count: 3 -> 2
    /// syncPoint.DecrementAndCheck(JobResult::Success()); // count: 2 -> 1
    /// syncPoint.DecrementAndCheck(JobResult::Failed("Error")); // count: 1 -> 0, status degraded
    ///
    /// // When counter reaches 0, continuations are activated or cancelled base on final status
    /// @endcode
    struct SyncPoint
    {
        // === Core Synchronization Data (Hot Path) ===

        /// @brief Number of dependencies remaining before sync point is signaled
        std::atomic<uint32_t> counter;

        /// @brief Aggregated status of all dependencies (can only degrade)
        std::atomic<JobResult::Status> status;

        // === Continuation Management (Hybrid Storage) ===

        /// @brief Number of inline continuation slots (optimized for common case)
        static constexpr size_t INLINE_CONTINUATIONS = 6;

        /// @brief Inline continuation storage for fast access (covers ~95% of use cases)
        std::array<std::atomic<JobHandle>, INLINE_CONTINUATIONS> inlineContinuations;

        /// @brief Number of continuations stored inline (0-6)
        std::atomic<uint8_t> inlineCount{0};

        /// @brief Overflow storage for rare cases with >6 continuations
        std::atomic<VAArray<JobHandle>*> overflowContinuations{nullptr};

        /// @brief Simple spinlock for overflow vector operations (rare case)
        std::atomic<bool> overflowLock{false};

        // === Metadata and Debug ===

        /// @brief Debug name for profiling and logging (should be static string)
        const char* debugName;

        /// @brief Timestamp when sync point was created
        std::chrono::high_resolution_clock::time_point creationTime;

        // === Constructors ===

        /// @brief Default constructor creates invalid sync point
        SyncPoint() = default;

        /// @brief Construct a SyncPoint with initial dependency count
        /// @param initialCount Number of dependencies that must complete before signaling
        /// @param name Debug name for the sync point (should be static string)
        SyncPoint(uint32_t initialCount, const char* name);

        /// @brief Destructor cleans up overflow storage if allocated
        ~SyncPoint();

        // Non-copyable and non-movable (contains atomic members)
        SyncPoint(const SyncPoint&) = delete;
        SyncPoint& operator=(const SyncPoint&) = delete;
        SyncPoint(SyncPoint&&) = delete;
        SyncPoint& operator=(SyncPoint&&) = delete;

        // === Core Synchronization Operations ===

        /// @brief Atomically decrement counter and check if sync point is signaled
        /// @param result Result to propagate (affects status if not Success)
        /// @return true if counter reached zero and sync point is now signaled
        ///
        /// This is the core synchronization primitive used by completing jobs.
        /// It atomically decrements the dependency counter and propagates status.
        /// When the counter reaches zero, the SyncPoint is considered signaled.
        ///
        /// Status propagation rules:
        /// - Success: No status change
        /// - Failed/Cancelled: Status degrades atomically (first failure wins)
        bool DecrementAndCheck(const JobResult& result);

        /// @brief Check if SyncPoint is currently signaled
        /// @return true if counter has reached zero
        bool IsSignaled() const
        {
            return counter.load(std::memory_order_acquire) == 0;
        }

        /// @brief Get current dependency count
        /// @return Number of dependencies still pending
        uint32_t GetPendingCount() const
        {
            return counter.load(std::memory_order_acquire);
        }

        /// @brief Get current status of the SyncPoint
        /// @return Current status (Success, Failed, or Cancelled)
        JobResult::Status GetStatus() const
        {
            return status.load(std::memory_order_acquire);
        }

        // === Continuation Management ===

        /// @brief Add a job continuation to this SyncPoint
        /// @param handle Handle to job that should be activated when sync point signals
        /// @return true if continuation was added successfully
        ///
        /// Continuations are jobs that depend on this SyncPoint. When the SyncPoint signals,
        /// all continuations are either activated (is status is Success) or cancelled
        /// (if status is Failed/Cancelled).
        ///
        /// Implementation uses hybrid storage:
        /// - First 6 continuations stored inline (lock-free)
        /// - Additional continuations stored in overflow vector (minimal locking)
        bool AddContinuation(JobHandle handle);

        /// @brief Get all continuation handles for processing
        /// @return Vector containing all continuation handles
        ///
        /// This method is used by the job scheduler to retrieve all continuations
        /// when the SyncPoint signals. If safely combines inline and overflow storage.
        VAArray<JobHandle> GetContinuations();

        // === Status and Timing Information ===

        /// @brief Get a human-readable status string
        /// @return String representation of current status
        const char* GetStatusString() const
        {
            switch (status.load(std::memory_order_relaxed))
            {
                case JobResult::Status::Success:
                    return "Success";
                case JobResult::Status::Failed:
                    return "Failed";
                case JobResult::Status::Cancelled:
                    return "Cancelled";
                default:
                    return "Unknown";
            }
        }

        /// @brief Get time elapsed since SyncPoint creation
        /// @return Duration since creation in nanoseconds
        std::chrono::nanoseconds GetElapsedTime() const
        {
            return std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::high_resolution_clock::now() - creationTime);
        }

        /// @brief Get debug name for logging and profiling
        /// @return Debug name string
        const char* GetDebugName() const
        {
            return debugName ? debugName : "UnnamedSyncPoint";
        }

    private:
        // === Private Helpers ===

        /// @brief Propagate failure status atomically
        /// @param failureStatus Status to propagate (Failed or Cancelled)
        ///
        /// This method ensures that once a SyncPoint fails, it cannot return to success.
        /// Only the first failure is recorded (atomic compare-exchange).
        void PropagateFailure(JobResult::Status failureStatus);

        /// @brief Add continuation to overflow storage (slow path)
        /// @param handle JobHandle to add to overflow
        /// @return true if successfully added
        ///
        /// This method handles the rare case where more than 6 continuations are needed.
        /// It uses a simple spinlock for synchronization since this path is uncommon.
        bool AddToOverflow(JobHandle handle);
    };
}
