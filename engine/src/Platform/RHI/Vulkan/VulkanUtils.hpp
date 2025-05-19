//
// Created by Michael Desmedt on 19/05/2025.
//
#pragma once
#include <vulkan/vulkan_core.h>

#include "Core/Logger.hpp"

namespace VoidArchitect::Platform
{
    std::string VulkanGetResultString(VkResult result, bool extended = true);

    bool VulkanCheckResult(VkResult result);

#define VA_VULKAN_CHECK_RESULT_CRITICAL(x) \
    { \
        auto result = x; \
        if (!VulkanCheckResult(result)) \
        {\
            auto errorString = VulkanGetResultString(result); \
            VA_ENGINE_CRITICAL("Vulkan error: {}", errorString); \
            throw std::runtime_error(errorString); \
        }\
    }

#define VA_VULKAN_CHECK_RESULT_WARN(x) \
    { \
        auto result = x; \
        if (!VulkanCheckResult(result)) \
        {\
            auto errorString = VulkanGetResultString(result); \
            VA_ENGINE_WARN("Vulkan error: {}", errorString); \
        }\
    }

#define VA_VULKAN_CHECK_RESULT(x) !VulkanCheckResult(x)
}
