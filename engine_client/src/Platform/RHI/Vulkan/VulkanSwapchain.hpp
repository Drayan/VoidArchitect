//
// Created by Michael Desmedt on 17/05/2025.
//
#pragma once

#include "VulkanDevice.hpp"

#include <vulkan/vulkan.h>

#include "VulkanImage.hpp"
#include "VulkanRhi.hpp"

namespace VoidArchitect::Platform
{
    class VulkanSwapchain
    {
    public:
        VulkanSwapchain(
            VulkanRHI& rhi,
            const std::unique_ptr<VulkanDevice>& device,
            VkAllocationCallbacks* allocator,
            VkSurfaceFormatKHR format,
            VkPresentModeKHR presentMode,
            VkExtent2D extent,
            VkFormat depthFormat);
        ~VulkanSwapchain();

        bool AcquireNextImage(
            uint64_t timeout,
            VkSemaphore semaphore,
            VkFence fence,
            uint32_t& out_imageIndex) const;
        void Present(VkQueue graphicsQueue, VkSemaphore renderComplete, uint32_t imageIndex) const;

    private:
        VkDevice m_Device;
        VkAllocationCallbacks* m_Allocator;
        VkSwapchainKHR m_Swapchain;

        VkSurfaceFormatKHR m_Format;
        VkPresentModeKHR m_PresentMode;
        VkExtent2D m_Extent;
        VkFormat m_DepthFormat;

        std::vector<VulkanImage> m_SwapchainImages;
        VulkanImage m_DepthImage;
        uint32_t m_MaxFrameInFlight = 2; // Support triple-buffering by default
    };
} // VoidArchitect
