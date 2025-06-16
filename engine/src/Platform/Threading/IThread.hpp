//
// Created by Michael Desmedt on 16/06/2025.
//
#pragma once

#include "ThreadTypes.hpp"

namespace VoidArchitect::Platform
{
    /// Base interface for cross-platform thread abstraction
    class IThread
    {
    public:
        virtual ~IThread() = default;

        /// Start the thread with a given function and optional name
        /// @param func The function to execute on the new thread
        /// @param name Optional name for debugging/profiling purposes
        /// @return true if the thread was started successfully, false otherwise
        virtual bool Start(ThreadFunction func, const std::string& name) = 0;

        /// Wait for the thread to complete execution
        /// This will block the calling thread until this thread finishes
        virtual void Join() = 0;

        /// Detach the thread, allowing it to run independently
        /// After detaching; the thread cannot be joined
        virtual void Detach() = 0;

        /// Set the thread's execution priority
        /// @param priority The desired priority level
        virtual void SetPriority(ThreadPriority priority) = 0;

        /// Set the thread's CPU affinity mask
        /// @param cpuMask Bitmask indicating which CPUs this thread can run on
        virtual void SetAffinity(uint64_t cpuMask) = 0;

        /// Set the thread's stack size
        /// @param stackSize Size in bytes for the thread stack
        /// @remark This must be called before `Start()`, otherwise it's ignored.
        virtual void SetStackSize(size_t stackSize) = 0;

        /// Request the thread to stop gracefully
        /// This sets an internal flag that the thread can check
        virtual void RequestStop() = 0;

        /// Check if a stop has been requested
        /// @return true if `RequestStop()`was called, false otherwise
        virtual bool ShouldStop() const = 0;

        /// Check if the thread is currently running
        /// @return true if the thread is active, false otherwise
        virtual bool IsRunning() const = 0;

        /// Check if the thread can be joined
        /// @return true if `Join()` can be called, false otherwise
        virtual bool IsJoinable() const = 0;

        /// Get the handle for this thread
        /// @return The thread handle, or InvalidThreadHandle if not started
        virtual ThreadHandle GetHandle() const = 0;

        /// Get the name assigned to this thread
        /// @return The thread name string, if one is assigned, an empty string otherwise
        virtual const std::string& GetName() = 0;
    };
}
