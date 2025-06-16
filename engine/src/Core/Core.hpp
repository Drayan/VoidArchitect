//
// Created by Michael Desmedt on 12/05/2025.
//
#pragma once

// Platform detection
#if (defined(_MSC_VER) || defined(WIN32) || defined(_WIN32)) && !defined(VOID_ARCH_PLATFORM_WINDOWS)
#define VOID_ARCH_PLATFORM_WINDOWS
#elif defined(__APPLE__) && !defined(VOID_ARCH_PLATFORM_MACOS)
#define VOID_ARCH_PLATFORM_MACOS
#elif defined(__linux__) && !defined(VOID_ARCH_PLATFORM_LINUX)
#define VOID_ARCH_PLATFORM_LINUX
#else
#error "Unsupported platform!"
#endif

// Export/import macros based on the platform
#if defined(VOID_ARCH_PLATFORM_WINDOWS)
#ifdef VOID_ARCH_ENGINE_EXPORTS
#define VA_API __declspec(dllexport)
#else
#define VA_API __declspec(dllimport)
#endif
#else
#ifdef VOID_ARCH_ENGINE_EXPORTS
#define VA_API __attribute__((visibility("default")))
#else
#define VA_API
#endif
#endif

// --- Assertions ---
#ifdef VA_ENABLE_ASSERTS
#ifdef VOID_ARCH_PLATFORM_WINDOWS
#define VA_ASSERT(x, ...)                                                                          \
    {                                                                                              \
        if (!(x))                                                                                  \
        {                                                                                          \
            VA_APP_CRITICAL("Assertion Failed: {0}", __VA_ARGS__);                                 \
            __debugbreak();                                                                        \
        }                                                                                          \
    }
#define VA_ENGINE_ASSERT(x, ...)                                                                   \
    {                                                                                              \
        if (!(x))                                                                                  \
        {                                                                                          \
            VA_ENGINE_CRITICAL("Assertion Failed: {0}", __VA_ARGS__);                              \
            __debugbreak();                                                                        \
        }                                                                                          \
    }
#else
#define VA_ASSERT(x, ...)                                                                          \
    {                                                                                              \
        if (!(x))                                                                                  \
        {                                                                                          \
            VA_APP_CRITICAL("Assertion Failed: {0}", __VA_ARGS__);                                 \
            __builtin_debugtrap();                                                                 \
        }                                                                                          \
    }
#define VA_ENGINE_ASSERT(x, ...)                                                                   \
    {                                                                                              \
        if (!(x))                                                                                  \
        {                                                                                          \
            VA_ENGINE_CRITICAL("Assertion Failed: {0}", __VA_ARGS__);                              \
            __builtin_debugtrap();                                                                 \
        }                                                                                          \
    }
#endif
#else
#define VA_ASSERT(x, ...)
#define VA_ENGINE_ASSERT(x, ...)
#endif

#define BIT(x) (1 << x)
