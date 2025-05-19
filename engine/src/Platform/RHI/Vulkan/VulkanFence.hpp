//
// Created by Michael Desmedt on 19/05/2025.
//
#pragma once
#include <vulkan/vulkan.h>

namespace VoidArchitect::Platform
{
    class VulkanDevice;

    class VulkanFence
    {
    public:
        VulkanFence() = default;
        VulkanFence(
            const std::unique_ptr<VulkanDevice>& device,
            VkAllocationCallbacks* allocator,
            bool createSignaled = false);
        ~VulkanFence();

        bool Wait(uint64_t timeout);
        void Reset();
        VkFence GetHandle() const { return m_Fence; };

    private:
        VkDevice m_Device;
        VkAllocationCallbacks* m_Allocator;

        bool m_Signaled = false;
        VkFence m_Fence;
    };
} // VoidArchitect
