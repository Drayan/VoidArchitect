//
// Created by Michael Desmedt on 16/06/2025.
//
#pragma once

namespace VoidArchitect::Platform
{
    /// Priority levels for thread scheduling
    enum class ThreadPriority
    {
        Low, ///< Below normal priority
        Normal, ///< Normal priority (default)
        High, ///< Above normal priority
        Critical ///< Highest priority (use sparingly)
    };

    /// Function signature for thread entry points
    using ThreadFunction = std::function<void()>;

    /// Unique handle for threads
    using ThreadHandle = uint32_t;

    /// Invalid thread handle
    constexpr ThreadHandle InvalidThreadHandle = std::numeric_limits<uint32_t>::max();
}
