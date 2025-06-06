//
// Created by Michael Desmedt on 19/05/2025.
//
#pragma once
#include <vulkan/vulkan_core.h>

#include "Core/Logger.hpp"

namespace VoidArchitect
{
    namespace Renderer
    {
        enum class StoreOp;
        enum class LoadOp;
        enum class TextureFormat;
    }

    namespace Resources
    {
        enum class ShaderStage;
    }

    enum class ResourceBindingType;
} // namespace VoidArchitect

namespace VoidArchitect::Platform
{
    // === Engine -> Vulkan translation functions ===
    VkDescriptorType TranslateEngineResourceTypeToVulkan(ResourceBindingType type);
    VkShaderStageFlagBits TranslateEngineShaderStageToVulkan(Resources::ShaderStage stage);
    VkFormat TranslateEngineTextureFormatToVulkan(Renderer::TextureFormat format);
    VkAttachmentLoadOp TranslateEngineLoadOpToVulkan(Renderer::LoadOp op);
    VkAttachmentStoreOp TranslateEngineStoreOpToVulkan(Renderer::StoreOp op);

    // === Vulkan -> Engine translation functions ===
    Renderer::TextureFormat TranslateVulkanTextureFormatToEngine(VkFormat format);
    Renderer::LoadOp TranslateVulkanLoadOpToEngine(VkAttachmentLoadOp op);
    Renderer::StoreOp TranslateVulkanStoreOpToEngine(VkAttachmentStoreOp op);

    std::string VulkanGetResultString(VkResult result, bool extended = true);

    bool VulkanCheckResult(VkResult result);

#define VA_VULKAN_CHECK_RESULT_CRITICAL(x)                                                         \
    {                                                                                              \
        auto result = x;                                                                           \
        if (!VulkanCheckResult(result))                                                            \
        {                                                                                          \
            auto errorString = VulkanGetResultString(result);                                      \
            VA_ENGINE_CRITICAL("Vulkan error: {}", errorString);                                   \
            throw std::runtime_error(errorString);                                                 \
        }                                                                                          \
    }

#define VA_VULKAN_CHECK_RESULT_WARN(x)                                                             \
    {                                                                                              \
        auto result = x;                                                                           \
        if (!VulkanCheckResult(result))                                                            \
        {                                                                                          \
            auto errorString = VulkanGetResultString(result);                                      \
            VA_ENGINE_WARN("Vulkan error: {}", errorString);                                       \
        }                                                                                          \
    }

#define VA_VULKAN_CHECK_RESULT(x) !VulkanCheckResult(x)
} // namespace VoidArchitect::Platform
