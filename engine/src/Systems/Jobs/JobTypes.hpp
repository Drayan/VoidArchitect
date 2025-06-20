//
// Created by Michael Desmedt on 17/06/2025.
//
#pragma once

#include "Core/Handle.hpp"
#include "Platform/RHI/IRenderingHardware.hpp"
#include "Platform/Threading/Thread.hpp"
#include "Platform/Threading/ThreadFactory.hpp"

namespace VoidArchitect::Jobs
{
    // === Configuration Constants ===

    /// @brief Maximum number of jobs that can exist simultaneously in the system
    constexpr size_t MAX_JOBS = 8192;

    /// @brief Maximum number of worker threads supported by the job system
    constexpr size_t MAX_WORKERS = 32;

    /// @brief Default number of dependency slots per job for optimal memory layout
    constexpr size_t DEFAULT_DEPENDENCY_CAPACITY = 4;

    // === Handle Types ===

    /// @brief Handle for referencing jobs with generation-based validation
    using JobHandle = Handle<class JobTag>;

    /// @brief Invalid job handle constant for convenience
    constexpr JobHandle InvalidJobHandle = JobHandle{};

    // === Job Result System ===

    /// @brief Result structure returned by job execution
    ///
    /// JobResult provides a standardized way to report job completion status
    /// and error information. This enables robust error handling and debugging
    /// throughout the job system.
    ///
    /// Usage examples:
    /// @code
    /// // Successful job
    /// return JobResult::Success();
    ///
    /// // Failed job with error message
    /// return JobResult::Failed("File not found: config.json");
    ///
    /// // Cancelled job
    /// return JobResult::Cancelled("User requested shutdown");
    /// @endcode
    struct JobResult
    {
        /// @brief Possible job execution states
        enum class Status : uint8_t
        {
            Success = 0, ///< Job completed successfully
            Failed, ///< Job failed with error
            Cancelled, ///< Job was cancelled before completion
        };

        /// @brief Current status of the job
        Status status;

        /// @brief Optional error message for failed/cancelled jobs
        std::string errorMessage;

        // --- Constructors ---

        /// @brief Default constructor creates successful result
        JobResult()
            : status(Status::Success)
        {
        }

        /// @brief Construct result with specific status and message
        /// @param s Job status
        /// @param msg Error message (optional)
        explicit JobResult(const Status s, const std::string& msg = "")
            : status(s),
              errorMessage(msg)
        {
        }

        // --- Factory Methods ---

        /// @brief Create a successful job result
        /// @return JobResult indicating success
        static JobResult Success() { return JobResult{Status::Success}; }

        /// @brief Create a failed job result with an error message
        /// @param message Description of the failure
        /// @return JobResult indicating failure
        static JobResult Failed(const std::string& message)
        {
            return JobResult{Status::Failed, message};
        }

        /// @brief Create a cancelled job result with reason
        /// @param message Description of why the job was cancelled
        /// @return JobResult indicating cancellation
        static JobResult Cancelled(const std::string& message = "Job was cancelled")
        {
            return JobResult{Status::Cancelled, message};
        }

        // --- Status Checking ---

        /// @brief Check if a job completed successfully
        /// @return true if status is Success
        bool IsSuccess() const { return status == Status::Success; }

        /// @brief Check if a job failed
        /// @return true if status is Failed
        bool IsFailed() const { return status == Status::Failed; }

        /// @brief Check if a job was cancelled
        /// @return true if status is Cancelled
        bool IsCancelled() const { return status == Status::Cancelled; }

        /// @brief Get a human-readable status string
        /// @return String representation of the current status
        std::string GetStatusString() const
        {
            switch (status)
            {
                case Status::Success:
                    return "Success";
                case Status::Failed:
                    return "Failed";
                case Status::Cancelled:
                    return "Cancelled";
                default:
                    return "Unknown";
            }
        }

        // --- Job Priority System ---

        /// @brief Job execution priority levels
        ///
        /// Higher priority jobs have a greater chance of be executed before lower priority
        /// jobs within the same dependency constraints. Critical jobs should be used sparingly
        /// for time-sensitive operations like input processing or frame sync.
        enum class JobPriority : uint8_t
        {
            Low = 0, ///< Background task, asset streaming, analytics
            Normal = 1, ///< Standard game logic, physics simulation, rendering
            High = 2, ///< Time-sensitive operations, user input response
            Critical = 3, ///< Frame synchronization, emergency shutdown
        };

        /// @brief Get a human-readable priority name
        /// @param priority Priority level to convert
        /// @return String representation of the priority
        inline const char* GetPriorityString(JobPriority priority) const
        {
            switch (priority)
            {
                case JobPriority::Low:
                    return "Low";
                case JobPriority::Normal:
                    return "Normal";
                case JobPriority::High:
                    return "High";
                case JobPriority::Critical:
                    return "Critical";
                default:
                    return "Unknown";
            }
        }

        // --- Job Function Types ---

        /// @brief Function signature for job execution
        ///
        /// Job functions should be self-contained and thread-safe. They receive
        /// no parameters and return a JobResult indicating success or failure.
        using JobFunction = std::function<JobResult()>;

        /// @brief Function signature for void jobs (convenience)
        ///
        /// Void jobs are automatically wrapped to return JobResult::Success()
        /// upon completion. Use for jobs that cannot fail or handle errors internally.
        using VoidJobFunction = std::function<void()>;

        // --- Job State Tracking ---

        /// @brief Atomic job execution state for thread-safe status tracking
        enum class JobState : uint8_t
        {
            Pending = 0, ///< Job is waiting for dependencies or worker availability
            Ready, ///< Job dependencies satisfied, ready for execution
            Executing, ///< Job is currently being executed by a worker
            Completed, ///< Job execution finished (check result for success/failure)
            Cancelled, ///< Job was cancelled before execution
        };

        /// @brief Get human-readable state string
        /// @param state Job state to convert
        /// @return String representation of the state
        inline const char* GetJobStateString(JobState state) const
        {
            switch (state)
            {
                case JobState::Pending:
                    return "Pending";
                case JobState::Ready:
                    return "Ready";
                case JobState::Executing:
                    return "Executing";
                case JobState::Completed:
                    return "Completed";
                case JobState::Cancelled:
                    return "Cancelled";
                default:
                    return "Unknown";
            }
        }

        // --- Core Job Structure ---

        /// @brief Core job structure containing execution data and metadata
        ///
        /// Job represents a unit of work that can be executed by the job system.
        /// Jobs support dependency management through atomic counters, priority-based
        /// scheduling, and comprehensive state tracking for debugging and profiling.
        ///
        /// Memory layout is optimized for cache efficiency with hot data (state, dependencies)
        /// placed at the beginning of the structure.
        struct Job
        {
            // --- Hot Data (frequently accessed during execution) ---

            /// @brief Current execution state of the job
            std::atomic<JobState> state{JobState::Pending};

            /// @brief Number of incomplete dependencies (the job becomes ready when this reaches 0)
            std::atomic<uint32_t> dependencyCount{0};

            /// @brief Job execution priority for scheduling
            JobPriority priority{JobPriority::Normal};

            /// @brief Preferred worker thread ID (UINT32_MAX = any worker)
            uint32_t workerAffinity{UINT32_MAX};

            // --- Execution Data ---

            /// @brief Function to execute for this job
            JobFunction executeFunction;

            /// @brief Result of job execution (valid only after completion)
            JobResult result;

            // --- Metadata and Debug ---

            /// @brief Debug name for profiling and loggin (should be static string)
            const char* debugName{"UnamedJob"};

            /// @brief Handle of the job that spawned this job (for hierarchical tracking)
            JobHandle parentJob{InvalidJobHandle};

            /// @brief Thread Id that submitted this job (for debug)
            Platform::ThreadHandle submitterThread;

            // --- Timing information ---

            /// @brief Timestamp when job was submitted to the system
            std::chrono::high_resolution_clock::time_point submitTime;

            /// @brief Timestamp when job execution started
            std::chrono::high_resolution_clock::time_point startTime;

            /// @brief Timestamp when job execution completed
            std::chrono::high_resolution_clock::time_point endTime;

            // --- Constructors ---

            /// @brief Default constructor (creates an empty job)
            Job() = default;

            /// @brief Construct a job with function and metadata
            /// @param func Function to execute
            /// @param name Debug name for the job
            /// @param prio Job priority
            /// @param affinity Preferred worker Id (optional)
            Job(
                JobFunction func,
                const char* name,
                const JobPriority prio = JobPriority::Normal,
                const uint32_t affinity = UINT32_MAX)
                : priority(prio),
                  workerAffinity(affinity),
                  executeFunction(std::move(func)),
                  debugName(name),
                  submitterThread(Platform::Thread::GetCurrentThreadHandle()),
                  submitTime(std::chrono::high_resolution_clock::now())
            {
            }

            /// @brief Construct job from void function (automatically wrapped)
            /// @param func Void function to execute
            /// @param name Debug name for the job
            /// @param prio Job priority
            /// @param affinity Preferred worker ID (optional)
            Job(
                VoidJobFunction func,
                const char* name,
                const JobPriority prio = JobPriority::Normal,
                const uint32_t affinity = UINT32_MAX)
                : priority(prio),
                  workerAffinity(affinity),
                  debugName(name),
                  submitterThread(Platform::Thread::GetCurrentThreadHandle()),
                  submitTime(std::chrono::high_resolution_clock::now())
            {
                // Wrap a void function to return success
                executeFunction = [func = std::move(func)]() -> JobResult
                {
                    func();
                    return JobResult::Success();
                };
            }

            // --- State Management ---

            /// @brief Check if the job is ready for execution
            /// @return true if dependencies are satisfied and job can be executed
            bool isReady() const
            {
                return state.load() == JobState::Ready || (state.load() == JobState::Pending &&
                    dependencyCount.load() == 0);
            }

            /// @brief Check if the job is currently executing
            /// @return true if the job is being processed by a worker thread
            bool IsExecuting() const
            {
                return state.load() == JobState::Executing;
            }

            /// @brief Check if job was cancelled
            /// @return true if job was cancelled before execution
            bool IsCancelled() const
            {
                return state.load() == JobState::Cancelled;
            }

            /// @brief Try to transition job state atomically
            /// @param expectedState Current expected state
            /// @param newState Desired new state
            /// @return true if transition succeeded
            bool TryTransitionState(JobState expectedState, const JobState newState)
            {
                return state.compare_exchange_weak(expectedState, newState);
            }

            /// @brief Decrement dependency counter atomically
            /// @return New dependency count after decrement
            uint32_t DecrementDependencies()
            {
                return dependencyCount.fetch_sub(1, std::memory_order_acq_rel) - 1;
            }

            /// @brief Add dependencies to this job
            /// @param count Number of dependencies to add
            void AddDependencies(const uint32_t count)
            {
                dependencyCount.fetch_add(count, std::memory_order_acq_rel);
            }

            // --- Timing Utilities ---

            /// @brief Mark job execution as started
            void MarkExecutionStart()
            {
                startTime = std::chrono::high_resolution_clock::now();
            }

            /// @brief Mark job execution as completed
            void MarkExecutionEnd()
            {
                endTime = std::chrono::high_resolution_clock::now();
            }

            /// @brief Get total time from submission to completion
            /// @return Duration in nanoseconds
            std::chrono::nanoseconds GetTotalTime() const
            {
                if (submitTime == std::chrono::high_resolution_clock::time_point{} || endTime ==
                    std::chrono::high_resolution_clock::time_point{})
                {
                    return std::chrono::nanoseconds::zero();
                }

                return std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - submitTime);
            }

            /// @brief Get actual execution time (excluding queue time)
            /// @return Duration in nanoseconds
            std::chrono::nanoseconds GetExecutionTime() const
            {
                if (startTime == std::chrono::high_resolution_clock::time_point{} || endTime ==
                    std::chrono::high_resolution_clock::time_point{})
                {
                    return std::chrono::nanoseconds::zero();
                }

                return std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime);
            }

            /// @brief Get time spent waiting in queue
            /// @return Duration in nanoseconds
            std::chrono::nanoseconds GetQueueTime() const
            {
                if (submitTime == std::chrono::high_resolution_clock::time_point{} || startTime ==
                    std::chrono::high_resolution_clock::time_point{})
                {
                    return std::chrono::nanoseconds::zero();
                }

                return std::chrono::duration_cast<std::chrono::nanoseconds>(startTime - submitTime);
            }
        };

        // --- Helper Functions ---

        /// @brief Create a JobFunction from a void lambda for convenience
        /// @tparam F Function type (deduced)
        /// @param func Void function or lambda
        /// @return JobFunction that wraps the void function
        template <typename F>
        JobFunction MakeJobFunction(F&& func)
        {
            return [func = std::forward<F>(func)]() -> JobResult
            {
                func();
                return JobResult::Success();
            };
        }

        /// @brief Create a failing JobFunction for testing purposes
        /// @param errorMessage Error message to include in a result
        /// @return JobFunction that always fails with the given message
        inline JobFunction MakeFailingJob(const char* errorMessage)
        {
            return [errorMessage]() -> JobResult
            {
                return JobResult::Failed(errorMessage);
            };
        }
    };
}
