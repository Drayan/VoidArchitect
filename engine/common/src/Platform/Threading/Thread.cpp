//
// Created by Michael Desmedt on 16/06/2025.
//
#include "Thread.hpp"

#include "VoidArchitect/Engine/Common/Logger.hpp"

#ifdef VOID_ARCH_PLATFORM_WINDOWS
#include <Windows.h>
#include <processthreadsapi.h>
#elif defined(VOID_ARCH_PLATFORM_LINUX) || defined(VOID_ARCH_PLATFORM_MACOS)
#include <pthread.h>
#include <sched.h>
#ifdef VOID_ARCH_PLATFORM_LINUX
#include <sys/prctl.h>
#endif
#endif

namespace VoidArchitect::Platform
{
#ifdef VOID_ARCH_PLATFORM_WINDOWS
    using NativeThreadHandle = HANDLE;
#elif defined(VOID_ARCH_PLATFORM_LINUX) || defined(VOID_ARCH_PLATFORM_MACOS)
    using NativeThreadHandle = pthread_t;
#else
    using NativeThreadHandle = std::thread::id;
#endif

    /// Global thread registry for unified thread ID management
    struct ThreadRegistry
    {
        VAHashMap<ThreadHandle, NativeThreadHandle> activeThreads;
        std::atomic<ThreadHandle> nextThreadId{0};
        std::mutex registryMutex;

        ThreadHandle RegisterThread(const NativeThreadHandle threadId)
        {
            std::lock_guard<std::mutex> lock(registryMutex);
            auto handle = nextThreadId.fetch_add(1);

            activeThreads[handle] = threadId;
            return handle;
        }

        void UnregisterThread(ThreadHandle handle)
        {
            std::lock_guard<std::mutex> lock(registryMutex);
            activeThreads.erase(handle);
        }

        ThreadHandle FindThreadHandle(const NativeThreadHandle threadId)
        {
            std::lock_guard<std::mutex> lock(registryMutex);
            for (const auto& [handle, id] : activeThreads)
            {
                if (id == threadId)
                {
                    return handle;
                }
            }
            return InvalidThreadHandle;
        }

        static NativeThreadHandle GetCurrentNativeHandle()
        {
#ifdef VOID_ARCH_PLATFORM_WINDOWS
            return GetCurrentThread();
#elif defined(VOID_ARCH_PLATFORM_LINUX) || defined(VOID_ARCH_PLATFORM_MACOS)
            return pthread_self();
#else
            return std::this_thread::get_id();
#endif
        }
    };

    /// Global thread registry instance
    static ThreadRegistry g_ThreadRegistry;

    thread_local Thread* g_CurrentThread = nullptr;

    Thread::~Thread()
    {
        if (m_Thread.joinable())
        {
            VA_ENGINE_WARN(
                "[Thread] Thread '{}' was not properly joined before destruction. Forcing detach.",
                m_Name);
            m_Thread.detach();
        }
    }

    void Thread::SetCurrentThreadName(const std::string& name)
    {
        if (name.empty())
        {
            VA_ENGINE_WARN("[Thread] Attempted to set empty thread name.");
            return;
        }

#ifdef VOID_ARCH_PLATFORM_WINDOWS
        std::wstring wideName(name.begin(), name.end()); SetThreadDescription(
            GetCurrentThread(),
            wideName.c_str());
#elif defined(VOID_ARCH_PLATFORM_MACOS)
        pthread_setname_np(name.c_str());
#elif defined(VOID_ARCH_PLATFORM_LINUX)
        prctl(PR_SET_NAME, name.c_str(), 0, 0, 0);
#endif

        VA_ENGINE_DEBUG("[Thread] Set current thread name to: {}.", name);
    }

    void Thread::Yield()
    {
        std::this_thread::yield();
    }

    void Thread::Sleep(uint32_t milliseconds)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    }

    ThreadHandle Thread::GetCurrentThreadHandle()
    {
        const auto nativeHandle = ThreadRegistry::GetCurrentNativeHandle();
        return g_ThreadRegistry.FindThreadHandle(nativeHandle);
    }

    bool Thread::Start(ThreadFunction func, const std::string& name)
    {
        if (m_IsRunning.load())
        {
            VA_ENGINE_ERROR("[Thread] Thread '{}' is already running.", m_Name);
            return false;
        }

        if (!func)
        {
            VA_ENGINE_ERROR("[Thread] Cannot start thread with null function !");
            return false;
        }

        m_UserFunction = std::move(func);
        m_Name = name.empty() ? "UnnamedThread" : name;
        m_ShouldStop.store(false);

        try
        {
            m_Thread = std::thread(&Thread::ThreadEntryPoint, this);
            return true;
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_ERROR("[Thread] Failed to start thread '{}': {}", m_Name, e.what());
            return false;
        }
    }

    void Thread::Join()
    {
        if (m_Thread.joinable())
        {
            m_Thread.join();
        }
        else
        {
            VA_ENGINE_WARN("[Thread] Attempted to join non-joinable thread '{}'.", m_Name);
        }
    }

    void Thread::Detach()
    {
        if (m_Thread.joinable())
        {
            m_Thread.detach();
        }
        else
        {
            VA_ENGINE_WARN("[Thread] Attempted to detach non-joinable thread '{}'.", m_Name);
        }
    }

    void Thread::SetPriority(ThreadPriority priority)
    {
        m_Priority = priority;

        // If the thread is already running, apply the setting immediately
        if (m_IsRunning.load())
        {
            ApplyThreadSettings();
        }
    }

    void Thread::SetAffinity(uint64_t cpuMask)
    {
        m_CpuMask = cpuMask;

        // If thread is already running, apply the setting immediately
        if (m_IsRunning.load())
        {
            ApplyThreadSettings();
        }
    }

    void Thread::SetStackSize(size_t stackSize)
    {
        if (m_IsRunning.load())
        {
            VA_ENGINE_WARN("[Thread] Cannot set stack size of running thread '{}'.", m_Name);
            return;
        }

        m_StackSize = stackSize;
    }

    void Thread::RequestStop()
    {
        m_ShouldStop.store(true);
    }

    bool Thread::ShouldStop() const
    {
        return m_ShouldStop.load();
    }

    bool Thread::IsRunning() const
    {
        return m_IsRunning.load();
    }

    bool Thread::IsJoinable() const
    {
        return m_Thread.joinable();
    }

    bool Thread::ShouldCurrentThreadStop()
    {
        // Check if we have a thread-local reference to the current thread
        if (g_CurrentThread != nullptr)
        {
            return g_CurrentThread->ShouldStop();
        }

        return false;
    }

    ThreadHandle Thread::GetHandle() const
    {
        return m_Handle;
    }

    const std::string& Thread::GetName()
    {
        return m_Name;
    }

    void Thread::ThreadEntryPoint()
    {
        m_IsRunning.store(true);

        // Set thread-local reference to current thread instance
        g_CurrentThread = this;

        // register this thread in the global registry using the native handle
        const auto nativeHandle = ThreadRegistry::GetCurrentNativeHandle();
        m_Handle = g_ThreadRegistry.RegisterThread(nativeHandle);

        // Apply thread settings (priority, affinity, name)
        ApplyThreadSettings();

        VA_ENGINE_DEBUG("[Thread] Thread '{}' started with ID: {}.", m_Name, m_Handle);

        try
        {
            // Execute user function
            if (m_UserFunction)
            {
                m_UserFunction();
            }
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_ERROR("[Thread] Exception in thread '{}' : {}", m_Name, e.what());
        }
        catch (...)
        {
            VA_ENGINE_ERROR("[Thread] Unknown exception in thread '{}'.", m_Name);
        }

        g_ThreadRegistry.UnregisterThread(m_Handle);

        // Clear thread-local reference
        g_CurrentThread = nullptr;

        m_IsRunning.store(false);
        VA_ENGINE_DEBUG("[Thread] Thread '{}' finished execution.", m_Name);
    }

    void Thread::ApplyThreadSettings()
    {
#ifdef VOID_ARCH_PLATFORM_WINDOWS
        HANDLE threadHandle = GetCurrentThread();

        // Set thread name
        if (!m_Name.empty())
        {
            std::wstring wideName(m_Name.begin(), m_Name.end());
            SetThreadDescription(threadHandle, wideName.c_str());
        }

        // Set thread priority
        auto winPriority = THREAD_PRIORITY_NORMAL; switch (m_Priority)
        {
            case ThreadPriority::Low:
                winPriority = THREAD_PRIORITY_BELOW_NORMAL;
                break;
            case ThreadPriority::High:
                winPriority = THREAD_PRIORITY_ABOVE_NORMAL;
                break;
            case ThreadPriority::Normal:
                winPriority = THREAD_PRIORITY_NORMAL;
                break;
            case ThreadPriority::Critical:
                winPriority = THREAD_PRIORITY_HIGHEST;
                break;
        } SetThreadPriority(threadHandle, winPriority);

        // Set CPU affinity
        if (m_CpuMask != 0)
        {
            SetThreadAffinityMask(threadHandle, m_CpuMask);
        }

#elif defined(VOID_ARCH_PLATFORM_MACOS) || defined(VOID_ARCH_PLATFORM_LINUX)
        // Set thread name
        if (!m_Name.empty())
        {
#ifdef VOID_ARCH_PLATFORM_LINUX
            prctl(PR_SET_NAME, m_Name.c_str(), 0, 0, 0);
#elif defined(VOID_ARCH_PLATFORM_MACOS)
            pthread_setname_np(m_Name.c_str());
#endif
        }

        // Set thread priority (simplified for POSIX)
        if (m_Priority != ThreadPriority::Normal)
        {
            struct sched_param param;
            int policy = SCHED_OTHER;

            switch (m_Priority)
            {
                case ThreadPriority::Low:
                    param.sched_priority = sched_get_priority_min(policy);
                    break;
                case ThreadPriority::High:
                case ThreadPriority::Critical:
                    param.sched_priority = sched_get_priority_max(policy);
                    break;
                default:
                    param.sched_priority = 0;
                    break;
            }

            pthread_setschedparam(pthread_self(), policy, &param);
        }

#ifdef VOID_ARCH_PLATFORM_LINUX
        // Set CPU affinity (Linux only)
        if (m_CpuMask != 0)
        {
            cpu_set_t cpuSet;
            CPU_ZERO(&cpuSet);

            for (int i = 0; i < 64; i++)
            {
                if (m_CpuMask & BIT(i))
                {
                    CPU_SET(i, &cpuSet);
                }
            }

            pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuSet);
        }
#endif
#endif
    }
}
