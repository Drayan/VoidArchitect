//
// Created by Michael Desmedt on 16/06/2025.
//
#include "Thread.hpp"
#include "ThreadFactory.hpp"

#if defined(VOID_ARCH_PLATFORM_MACOS) || defined(VOID_ARCH_PLATFORM_LINUX)
#include <unistd.h>
#endif

namespace VoidArchitect::Platform
{
    std::unique_ptr<IThread> ThreadFactory::CreateThread()
    {
        return std::make_unique<Thread>();
    }

    uint32_t ThreadFactory::GetHardwareConcurrency()
    {
        uint32_t concurrency = std::thread::hardware_concurrency();

        // Fallback if std::thread::hardware_concurrency returns 0
        if (concurrency == 0)
        {
#ifdef VOID_ARCH_PLATFORM_WINDOWS
            SYSTEM_INFO systemInfo; GetSystemInfo(&systemInfo); concurrency = static_cast<uint32_t>(
                systemInfo.dwNumberOfProcessors);
#elif defined(VOID_ARCH_PLATFORM_LINUX) || defined(VOID_ARCH_PLATFORM_MACOS)
            concurrency = static_cast<uint32_t>(sysconf(_SC_NPROCESSORS_ONLN));
#endif
        }

        return concurrency > 0 ? concurrency : 1;
    }
}
