//
// Created by Michael Desmedt on 17/05/2025.
//
#pragma once

#include "VulkanDevice.hpp"

#include <vulkan/vulkan.h>

#include "VulkanRhi.hpp"

namespace VoidArchitect::Platform
{
    class VulkanImage;

    class VulkanSwapchain
    {
    public:
        VulkanSwapchain(
            const VulkanRHI& rhi,
            const std::unique_ptr<VulkanDevice>& device,
            VkAllocationCallbacks* allocator,
            VkSurfaceFormatKHR format,
            VkPresentModeKHR presentMode,
            VkExtent2D extent);
        ~VulkanSwapchain();

    private:
        VkDevice m_Device;
        VkAllocationCallbacks* m_Allocator;
        VkSwapchainKHR m_Swapchain;

        VkSurfaceFormatKHR m_Format;
        VkPresentModeKHR m_PresentMode;
        VkExtent2D m_Extent;

        std::vector<VulkanImage> m_SwapchainImages;
    };
} // VoidArchitect
