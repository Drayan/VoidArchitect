//
// Created by Michael Desmedt on 17/05/2025.
//
#include "VulkanSwapchain.hpp"

#include "Core/Logger.hpp"
#include "VulkanImage.hpp"
#include "VulkanRenderPass.hpp"
#include "VulkanRhi.hpp"
#include "VulkanUtils.hpp"

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
        : m_Device(device),
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

        VA_VULKAN_CHECK_RESULT_CRITICAL(
            vkCreateSwapchainKHR(
                m_Device->GetLogicalDeviceHandle(), &swapchainCreateInfo, m_Allocator, &m_Swapchain
            ));

        // Retrieve the images from the swapchain...
        VA_VULKAN_CHECK_RESULT_WARN(
            vkGetSwapchainImagesKHR(
                m_Device->GetLogicalDeviceHandle(), m_Swapchain, &imageCount, nullptr));
        auto images = std::vector<VkImage>(imageCount);
        VA_VULKAN_CHECK_RESULT_CRITICAL(
            vkGetSwapchainImagesKHR(
                m_Device->GetLogicalDeviceHandle(), m_Swapchain, &imageCount, images.data()));

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
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }

    VulkanSwapchain::~VulkanSwapchain()
    {
        m_SwapchainImages.clear();

        if (m_Swapchain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(m_Device->GetLogicalDeviceHandle(), m_Swapchain, m_Allocator);
        }
    }

    // void VulkanSwapchain::RegenerateFramebuffers(
    //     const std::unique_ptr<VulkanRenderPass>& renderpass,
    //     uint32_t width,
    //     uint32_t height)
    // {
    //     m_RenderTargets.clear();
    //     m_RenderTargets.reserve(m_SwapchainImages.size());
    //     for (size_t i = 0; i < m_SwapchainImages.size(); i++)
    //     {
    //         auto config = Renderer::RenderTargetConfig{
    //             .Name = "SwapchainTarget_" + std::to_string(i),
    //             .Width = width,
    //             .Height = height,
    //             .Format = TranslateVulkanTextureFormatToEngine(m_Format.format),
    //             .IsMain = true
    //         };
    //
    //         std::vector attachments = {m_SwapchainImages[i].GetView(), m_DepthImage.GetView()};
    //         auto renderTarget = std::make_shared<VulkanRenderTarget>(
    //             config,
    //             m_Device->GetLogicalDeviceHandle(),
    //             m_Allocator);
    //
    //         renderTarget->CreateFramebuffer(renderpass, attachments);
    //
    //         m_RenderTargets.push_back(std::move(renderTarget));
    //     }
    //
    //     VA_ENGINE_DEBUG(
    //         "[VulkanSwapchain] All {} framebuffers regenerated.",
    //         m_RenderTargets.size());
    // }

    bool VulkanSwapchain::AcquireNextImage(
        const uint64_t timeout,
        const VkSemaphore semaphore,
        const VkFence fence,
        uint32_t& out_imageIndex) const
    {
        auto result = vkAcquireNextImageKHR(
            m_Device->GetLogicalDeviceHandle(),
            m_Swapchain,
            timeout,
            semaphore,
            fence,
            &out_imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            // TODO Recreate the swapchain.
            return false;
        }

        if (VA_VULKAN_CHECK_RESULT(result))
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
            // TODO Recreate the swapchain
        }
        else if (VA_VULKAN_CHECK_RESULT(result))
        {
            VA_ENGINE_CRITICAL("[VulkanSwapchain] Failed to present.");
            throw std::runtime_error("Failed to present.");
        }
    }

    void VulkanSwapchain::Recreate(
        VulkanRHI& rhi,
        const VkExtent2D extents,
        const VkFormat depthFormat)
    {
        VA_ENGINE_TRACE("[VulkanSwapchain] Recreating swapchain.");
        this->~VulkanSwapchain();
        new(this) VulkanSwapchain(
            rhi,
            m_Device,
            m_Allocator,
            m_Format,
            m_PresentMode,
            extents,
            depthFormat);
        VA_ENGINE_TRACE("[VulkanSwapchain] Swapchain recreated.");
    }
} // namespace VoidArchitect::Platform
