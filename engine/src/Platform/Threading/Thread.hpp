//
// Created by Michael Desmedt on 16/06/2025.
//
#pragma once

#include <thread>

#include "IThread.hpp"

namespace VoidArchitect::Platform
{
    class Thread final : public IThread
    {
    public:
        Thread() = default;
        ~Thread() override;

        /// Set the name of the current thread
        /// @param name The name to assign to the calling thread
        static void SetCurrentThreadName(const std::string& name);

        /// Yield execution of the current thread to other threads
        static void Yield();

        /// Put the current thread to sleep for a specified duration
        /// @param milliseconds Time to sleep in milliseconds
        static void Sleep(uint32_t milliseconds);

        /// Get the handle of the current thread
        /// @return The ThreadHandle of the calling thread
        static ThreadHandle GetCurrentThreadHandle();

        /// Check if the current thread should stop
        /// @return true if `RequestStop()` was called on the current thread, false otherwise
        static bool ShouldCurrentThreadStop();

        // ==========================================================
        // IThread implementation
        // ==========================================================
        bool Start(ThreadFunction func, const std::string& name) override;
        void Join() override;
        void Detach() override;

        void SetPriority(ThreadPriority priority) override;
        void SetAffinity(uint64_t cpuMask) override;
        void SetStackSize(size_t stackSize) override;

        void RequestStop() override;
        bool ShouldStop() const override;

        bool IsRunning() const override;
        bool IsJoinable() const override;

        ThreadHandle GetHandle() const override;
        const std::string& GetName() override;

    private:
        /// Internal thread entry point that wraps the user function
        void ThreadEntryPoint();

        /// Apply platform-specific thread settings
        void ApplyThreadSettings();

        std::thread m_Thread; ///< The underlying std::thread
        ThreadFunction m_UserFunction; ///< User-provided thread function
        std::string m_Name; ///< Thread name for debugging
        std::atomic<bool> m_IsRunning{false}; ///< Whether the thread is currently running
        std::atomic<bool> m_ShouldStop{false}; ///< Stop request flag
        ThreadHandle m_Handle{InvalidThreadHandle}; ///< Thread handle

        // Settings applied before thread start
        ThreadPriority m_Priority{ThreadPriority::Normal}; ///< Thread priority
        uint64_t m_CpuMask{0}; ///< CPU affinity mask, 0 means no affinity set
        size_t m_StackSize{0}; ///< Thread stack size, 0 means default size
    };
}
