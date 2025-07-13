//
// Created by Michael Desmedt on 19/05/2025.
//
#include "VulkanFence.hpp"

#include <VoidArchitect/Engine/Common/Logger.hpp>

#include "VulkanDevice.hpp"
#include "VulkanUtils.hpp"

namespace VoidArchitect::Platform
{
    VulkanFence::VulkanFence(
        const std::unique_ptr<VulkanDevice>& device,
        VkAllocationCallbacks* allocator,
        bool createSignaled)
        : m_Device(device->GetLogicalDeviceHandle()),
          m_Allocator(allocator),
          m_Signaled(createSignaled)
    {
        auto fenceCreateInfo = VkFenceCreateInfo{};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = createSignaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;

        VA_VULKAN_CHECK_RESULT_WARN(
            vkCreateFence(m_Device, &fenceCreateInfo, m_Allocator, &m_Fence));
        VA_ENGINE_TRACE("[VulkanFence] Fence created.");
    }

    VulkanFence::VulkanFence(VkDevice device, VkAllocationCallbacks* allocator, bool createSignaled)
        : m_Device{device},
          m_Allocator{allocator},
          m_Signaled{createSignaled}
    {
        auto fenceCreateInfo = VkFenceCreateInfo{};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = createSignaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;

        VA_VULKAN_CHECK_RESULT_WARN(
            vkCreateFence(m_Device, &fenceCreateInfo, m_Allocator, &m_Fence));
        VA_ENGINE_TRACE("[VulkanFence] Fence created.");
    }

    VulkanFence::~VulkanFence()
    {
        if (m_Fence != VK_NULL_HANDLE)
        {
            vkDestroyFence(m_Device, m_Fence, m_Allocator);
            VA_ENGINE_TRACE("[VulkanFence] Fence destroyed.");
        }
    }

    bool VulkanFence::Wait(const uint64_t timeout)
    {
        if (m_Signaled)
        {
            return true;
        }

        switch (const auto result = vkWaitForFences(m_Device, 1, &m_Fence, VK_TRUE, timeout))
        {
            case VK_SUCCESS:
            {
                m_Signaled = true;
                return true;
            }
            case VK_TIMEOUT: VA_ENGINE_WARN("[VulkanFence] Fence wait timed out.");
                break;

            case VK_ERROR_DEVICE_LOST: VA_ENGINE_WARN(
                    "[VulkanFence] Fence wait failed due to device lost.");
                break;

            case VK_ERROR_OUT_OF_HOST_MEMORY: VA_ENGINE_WARN(
                    "[VulkanFence] Fence wait failed due to out of host memory.");
                break;

            case VK_ERROR_OUT_OF_DEVICE_MEMORY: VA_ENGINE_WARN(
                    "[VulkanFence] Fence wait failed due to out of device memory.");
                break;

            default: VA_ENGINE_ERROR(
                    "[VulkanFence] Fence wait failed with {}.",
                    VulkanGetResultString(result));
                break;
        }

        return false;
    }

    void VulkanFence::Reset()
    {
        if (!m_Signaled) return;

        VA_VULKAN_CHECK_RESULT_WARN(vkResetFences(m_Device, 1, &m_Fence));
        m_Signaled = false;
    }
} // namespace VoidArchitect::Platform
