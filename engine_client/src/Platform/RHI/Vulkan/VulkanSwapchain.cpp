//
// Created by Michael Desmedt on 17/05/2025.
//
#include "VulkanSwapchain.hpp"

#include "VulkanImage.hpp"
#include "VulkanRhi.hpp"
#include "Core/Logger.hpp"

namespace VoidArchitect::Platform
{
    VulkanSwapchain::VulkanSwapchain(
        const VulkanRHI& rhi,
        const std::unique_ptr<VulkanDevice>& device,
        VkAllocationCallbacks* allocator,
        const VkSurfaceFormatKHR format,
        const VkPresentModeKHR presentMode,
        const VkExtent2D extent)
        : m_Device(device->GetLogicalDeviceHandle()),
          m_Allocator(allocator),
          m_Swapchain(VK_NULL_HANDLE),
          m_Format(format),
          m_PresentMode(presentMode),
          m_Extent(extent)
    {
        const auto capabilities = rhi.GetSwapchainCapabilities();
        uint32_t imageCount = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
            imageCount = capabilities.maxImageCount;

        auto swapchainCreateInfo = VkSwapchainCreateInfoKHR{};
        swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchainCreateInfo.surface = device->GetRefSurface();
        swapchainCreateInfo.minImageCount = imageCount;
        swapchainCreateInfo.imageFormat = format.format;
        swapchainCreateInfo.imageColorSpace = format.colorSpace;
        swapchainCreateInfo.imageExtent = extent;
        swapchainCreateInfo.imageArrayLayers = 1;
        swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        if (device->GetGraphicsFamily() != device->GetPresentFamily())
        {
            const uint32_t queueFamilyIndices[] = {
                device->GetGraphicsFamily().value(),
                device->GetPresentFamily().value()
            };
            swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            swapchainCreateInfo.queueFamilyIndexCount = 2;
            swapchainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else
            swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

        swapchainCreateInfo.preTransform = capabilities.currentTransform;
        swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchainCreateInfo.presentMode = presentMode;
        swapchainCreateInfo.clipped = VK_TRUE;
        swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(
            m_Device,
            &swapchainCreateInfo,
            m_Allocator,
            &m_Swapchain) != VK_SUCCESS)
        {
            VA_ENGINE_CRITICAL("[VulkanSwapchain] Failed to create the swapchain.");
            throw std::runtime_error("Failed to create the swapchain.");
        }

        // Retrieve the images from the swapchain...
        vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, nullptr);
        auto images = std::vector<VkImage>(imageCount);
        if (vkGetSwapchainImagesKHR(
            m_Device,
            m_Swapchain,
            &imageCount,
            images.data()) != VK_SUCCESS)
        {
            VA_ENGINE_CRITICAL("[VulkanSwapchain] Failed to retrieve the swapchain images.");
            throw std::runtime_error("Failed to retrieve the swapchain images.");
        }

        // And give them wrapped into our VulkanImage object that manages Image and ImageView.
        m_SwapchainImages.reserve(images.size());
        for (auto image : images)
        {
            // By default, VulkanImage will create an ImageView associated.
            m_SwapchainImages.emplace_back(
                device,
                m_Allocator,
                image,
                format.format);
        }
    }

    VulkanSwapchain::~VulkanSwapchain()
    {
        m_SwapchainImages.clear();

        if (m_Swapchain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(m_Device, m_Swapchain, m_Allocator);
        }
    }
} // VoidArchitect
