//
// Created by Michael Desmedt on 16/06/2025.
//
#pragma once
#include "IThread.hpp"

namespace VoidArchitect::Platform
{
    class ThreadFactory
    {
    public:
        /// Create a thread instance
        /// @return A new thread implementing the `IThread` interface
        static std::unique_ptr<IThread> CreateThread();

        /// Get the number of hardware threads available on this system
        /// @return Number of concurrent threads supported by the hardware
        static uint32_t GetHardwareConcurrency();
    };
}
