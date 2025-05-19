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
        VulkanRHI& rhi,
        const std::unique_ptr<VulkanDevice>& device,
        VkAllocationCallbacks* allocator,
        const VkSurfaceFormatKHR format,
        const VkPresentModeKHR presentMode,
        const VkExtent2D extent,
        const VkFormat depthFormat)
        : m_Device(device->GetLogicalDeviceHandle()),
          m_Allocator(allocator),
          m_Swapchain(VK_NULL_HANDLE),
          m_Format(format),
          m_PresentMode(presentMode),
          m_Extent(extent),
          m_DepthFormat(depthFormat),
          m_DepthImage{}
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
                format.format,
                VK_IMAGE_ASPECT_COLOR_BIT);
        }

        rhi.SetCurrentIndex(0);

        m_DepthImage = VulkanImage(
            rhi,
            device,
            m_Allocator,
            extent.width,
            extent.height,
            depthFormat,
            VK_IMAGE_ASPECT_DEPTH_BIT,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
    }

    VulkanSwapchain::~VulkanSwapchain()
    {
        m_SwapchainImages.clear();

        if (m_Swapchain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(m_Device, m_Swapchain, m_Allocator);
        }
    }

    void VulkanSwapchain::RegenerateFramebuffers(
        const std::unique_ptr<VulkanRenderpass>& renderpass,
        uint32_t width,
        uint32_t height)
    {
        m_Framebuffers.clear();
        m_Framebuffers.reserve(m_SwapchainImages.size());
        for (auto i = 0; i < m_SwapchainImages.size(); i++)
        {
            std::vector attachments = {
                m_SwapchainImages[i].GetView(),
                m_DepthImage.GetView()
            };
            m_Framebuffers.emplace_back(
                m_Device,
                m_Allocator,
                renderpass,
                width,
                height,
                attachments
            );
        }
    }

    bool VulkanSwapchain::AcquireNextImage(
        const uint64_t timeout,
        const VkSemaphore semaphore,
        const VkFence fence,
        uint32_t& out_imageIndex) const
    {
        auto result = vkAcquireNextImageKHR(
            m_Device,
            m_Swapchain,
            timeout,
            semaphore,
            fence,
            &out_imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            //TODO Recreate the swapchain.
            return false;
        }

        if (result != VK_SUCCESS && result != VK_TIMEOUT && result != VK_NOT_READY && result !=
            VK_SUBOPTIMAL_KHR)
        {
            VA_ENGINE_CRITICAL("[VulkanSwapchain] Failed to acquire next image.");
            throw std::runtime_error("Failed to acquire next image.");
        }

        return true;
    }

    void VulkanSwapchain::Present(
        const VkQueue graphicsQueue,
        const VkSemaphore renderComplete,
        const uint32_t imageIndex) const
    {
        auto presentInfo = VkPresentInfoKHR{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &renderComplete;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &m_Swapchain;
        presentInfo.pImageIndices = &imageIndex;

        auto result = vkQueuePresentKHR(graphicsQueue, &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        {
            //TODO Recreate the swapchain
        }
        else if (result != VK_SUCCESS)
        {
            VA_ENGINE_CRITICAL("[VulkanSwapchain] Failed to present.");
            throw std::runtime_error("Failed to present.");
        }
    }
} // VoidArchitect
