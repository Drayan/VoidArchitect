//
// Created by Michael Desmedt on 19/05/2025.
//
#pragma once
#include <vulkan/vulkan_core.h>

#include "Core/Logger.hpp"
#include "Systems/Renderer/RendererTypes.hpp"

namespace VoidArchitect
{
    namespace Resources
    {
        enum class ShaderStage;
        enum class TextureFilterMode;
        enum class TextureRepeatMode;
    }
} // namespace VoidArchitect

namespace VoidArchitect::Platform
{
    // === Engine -> Vulkan translation functions ===
    VkDescriptorType TranslateEngineResourceTypeToVulkan(Renderer::ResourceBindingType type);
    VkShaderStageFlagBits TranslateEngineShaderStageToVulkan(Resources::ShaderStage stage);
    VkFormat TranslateEngineTextureFormatToVulkan(Renderer::TextureFormat format);
    VkAttachmentLoadOp TranslateEngineLoadOpToVulkan(Renderer::LoadOp op);
    VkAttachmentStoreOp TranslateEngineStoreOpToVulkan(Renderer::StoreOp op);
    VkFilter TranslateEngineTextureFilterToVulkan(Resources::TextureFilterMode mode);
    VkSamplerAddressMode TranslateEngineTextureRepeatToVulkan(Resources::TextureRepeatMode mode);

    // === Vulkan -> Engine translation functions ===
    Renderer::TextureFormat TranslateVulkanTextureFormatToEngine(VkFormat format);
    Renderer::LoadOp TranslateVulkanLoadOpToEngine(VkAttachmentLoadOp op);
    Renderer::StoreOp TranslateVulkanStoreOpToEngine(VkAttachmentStoreOp op);

    constexpr size_t AlignUp(const size_t value, const size_t alignment)
    {
        return (value + alignment - 1) & ~(alignment - 1);
    }

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
