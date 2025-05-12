//
// Created by Michael Desmedt on 12/05/2025.
//
#pragma once

// Platform detection
#if defined(_MSC_VER) || defined(WIN32) || defined(_WIN32)
#define VOID_ARCH_PLATFORM_WINDOWS
#elif defined(__APPLE__)
#define VOID_ARCH_PLATFORM_MACOS
#elif defined(__linux__)
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
