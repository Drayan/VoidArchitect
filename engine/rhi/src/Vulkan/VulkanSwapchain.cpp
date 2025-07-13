//
// Created by Michael Desmedt on 17/05/2025.
//
#include "VulkanSwapchain.hpp"

#include <VoidArchitect/Engine/Common/Logger.hpp>

#include "VulkanRenderTargetSystem.hpp"
#include "VulkanResourceFactory.hpp"
#include "VulkanUtils.hpp"

namespace VoidArchitect::Platform
{
    VulkanSwapchain::VulkanSwapchain(
        const std::unique_ptr<VulkanDevice>& device,
        VkAllocationCallbacks* allocator,
        const uint32_t width,
        const uint32_t height)
        : m_Device(device),
          m_Allocator(allocator),
          m_Swapchain(VK_NULL_HANDLE),
          m_DepthRenderTarget(Resources::InvalidRenderTargetHandle)
    {
        Create(width, height);
    }

    VulkanSwapchain::~VulkanSwapchain()
    {
        Cleanup();
    }

    void VulkanSwapchain::Recreate(const uint32_t width, const uint32_t height)
    {
        VA_ENGINE_TRACE("[VulkanSwapchain] Recreating swapchain.");
        Cleanup();
        Create(width, height);
        VA_ENGINE_TRACE("[VulkanSwapchain] Swapchain recreated.");
    }

    void VulkanSwapchain::Cleanup()
    {
        m_SwapchainImages.clear();

        if (m_Swapchain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(m_Device->GetLogicalDeviceHandle(), m_Swapchain, m_Allocator);
        }
    }

    void VulkanSwapchain::Create(const uint32_t width, const uint32_t height)
    {
        QuerySwapchainCapabilities();
        VA_ENGINE_DEBUG("[VulkanSwapchain] Swapchain capabilities queried.");
        m_Extent = ChooseSwapchainExtent(width, height);
        m_Format = ChooseSwapchainFormat();
        m_PresentMode = ChooseSwapchainPresentMode();
        m_DepthFormat = ChooseDepthFormat();
        VA_ENGINE_DEBUG(
            "[VulkanSwapchain] Swapchain format, depth format, present mode and extent chosen.");

        uint32_t imageCount = m_Capabilities.minImageCount + 1;
        if (m_Capabilities.maxImageCount > 0 && imageCount > m_Capabilities.maxImageCount)
            imageCount = m_Capabilities.maxImageCount;

        auto swapchainCreateInfo = VkSwapchainCreateInfoKHR{};
        swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchainCreateInfo.surface = m_Device->GetRefSurface();
        swapchainCreateInfo.minImageCount = imageCount;
        swapchainCreateInfo.imageFormat = m_Format.format;
        swapchainCreateInfo.imageColorSpace = m_Format.colorSpace;
        swapchainCreateInfo.imageExtent = m_Extent;
        swapchainCreateInfo.imageArrayLayers = 1;
        swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        if (m_Device->GetGraphicsFamily() != m_Device->GetPresentFamily())
        {
            const uint32_t queueFamilyIndices[] = {
                m_Device->GetGraphicsFamily().value(),
                m_Device->GetPresentFamily().value()
            };
            swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            swapchainCreateInfo.queueFamilyIndexCount = 2;
            swapchainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

        swapchainCreateInfo.preTransform = m_Capabilities.currentTransform;
        swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchainCreateInfo.presentMode = m_PresentMode;
        swapchainCreateInfo.clipped = VK_TRUE;
        swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

        VA_VULKAN_CHECK_RESULT_CRITICAL(
            vkCreateSwapchainKHR( m_Device->GetLogicalDeviceHandle(), &swapchainCreateInfo,
                m_Allocator, &m_Swapchain ));

        // Retrieve the images from the swapchain...
        VA_VULKAN_CHECK_RESULT_WARN(
            vkGetSwapchainImagesKHR( m_Device->GetLogicalDeviceHandle(), m_Swapchain, &imageCount,
                nullptr));
        auto images = VAArray<VkImage>(imageCount);
        VA_VULKAN_CHECK_RESULT_CRITICAL(
            vkGetSwapchainImagesKHR( m_Device->GetLogicalDeviceHandle(), m_Swapchain, &imageCount,
                images.data()));

        // And give them wrapped into our VulkanImage object that manages Image and ImageView.
        m_SwapchainImages = images;

        CreateRenderTargetViews();
    }

    void VulkanSwapchain::CreateRenderTargetViews()
    {
        // NOTE: This method will be called at the start AND when resizing, we need to clear
        //  old render targets in the case of a resizing.
        for (const auto handle : m_ColorRenderTargets)
        {
            g_VkRenderTargetSystem->ReleaseRenderTarget(handle);
        }

        if (m_DepthRenderTarget != Resources::InvalidRenderTargetHandle)
        {
            g_VkRenderTargetSystem->ReleaseRenderTarget(m_DepthRenderTarget);
        }

        m_ColorRenderTargets.clear();
        m_ColorRenderTargets.reserve(m_SwapchainImages.size());

        // Create a RenderTarget for each ColorImage.
        uint32_t index = 0;
        for (const auto& image : m_SwapchainImages)
        {
            // Create a RenderTarget for the ColorImage.
            const auto renderTarget = g_VkRenderTargetSystem->CreateRenderTarget(
                "SwapchainColor_" + std::to_string(index++),
                image,
                m_Format.format);
            m_ColorRenderTargets.push_back(renderTarget);
        }

        // Create a RenderTarget for the DepthImage.
        Renderer::RenderTargetConfig depthConfig;
        depthConfig.name = "SwapchainDepth";
        depthConfig.usage = Renderer::RenderTargetUsage::DepthStencilAttachment;
        depthConfig.format = TranslateVulkanTextureFormatToEngine(m_DepthFormat);
        depthConfig.sizingPolicy = Renderer::RenderTargetConfig::SizingPolicy::Absolute;
        depthConfig.width = m_Capabilities.currentExtent.width;
        depthConfig.height = m_Capabilities.currentExtent.height;

        m_DepthRenderTarget = g_VkRenderTargetSystem->CreateRenderTarget(depthConfig);
    }

    void VulkanSwapchain::QuerySwapchainCapabilities()
    {
        const auto physicalDevice = m_Device->GetPhysicalDeviceHandle();
        const auto surface = m_Device->GetRefSurface();
        VA_VULKAN_CHECK_RESULT_WARN(
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &m_Capabilities));

        // --- Formats ---
        uint32_t formatCount;
        VA_VULKAN_CHECK_RESULT_WARN(
            vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr));

        if (formatCount != 0)
        {
            m_Formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(
                physicalDevice,
                surface,
                &formatCount,
                m_Formats.data());
        }
        else
        {
            VA_ENGINE_ERROR("[VulkanSwapchain] Failed to query surface formats.");
        }

        // --- Present modes ---
        uint32_t presentModeCount;
        VA_VULKAN_CHECK_RESULT_WARN(
            vkGetPhysicalDeviceSurfacePresentModesKHR( physicalDevice, surface, &presentModeCount,
                nullptr));

        if (presentModeCount != 0)
        {
            m_PresentModes.resize(presentModeCount);
            VA_VULKAN_CHECK_RESULT_WARN(
                vkGetPhysicalDeviceSurfacePresentModesKHR( physicalDevice, surface, &
                    presentModeCount, m_PresentModes.data()));
        }
        else
        {
            VA_ENGINE_ERROR("[VulkanSwapchain] Failed to query surface present modes.");
        }
    }

    VkSurfaceFormatKHR VulkanSwapchain::ChooseSwapchainFormat() const
    {
        constexpr VkFormat prefFormats[] = {
            VK_FORMAT_R8G8B8A8_SRGB,
            VK_FORMAT_B8G8R8A8_SRGB,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_FORMAT_B8G8R8A8_UNORM,
        };
        for (const auto& availableFormat : m_Formats)
        {
            for (const auto& prefFormat : prefFormats)
            {
                if (availableFormat.format == prefFormat && availableFormat.colorSpace ==
                    VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                {
                    return availableFormat;
                }
            }
        }

        VA_ENGINE_WARN(
            "[VulkanRHI] No suitable swapchain format found, choosing a default one : {}.",
            static_cast<uint32_t>(m_Formats[0].format));

#ifdef DEBUG
        // In debug build, print the list of available formats.
        VA_ENGINE_DEBUG("[VulkanRHI] Available formats:");
        for (const auto& [format, colorSpace] : m_Formats)
        {
            VA_ENGINE_DEBUG(
                "- {}/{}",
                static_cast<uint32_t>(format),
                static_cast<uint32_t>(colorSpace));
        }
#endif

        return m_Formats[0]; // If a required format is not found, we will use the first one.
    }

    VkPresentModeKHR VulkanSwapchain::ChooseSwapchainPresentMode() const
    {
        for (const auto& availablePresentMode : m_PresentModes)
        {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                return availablePresentMode;
            }
        }

        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D VulkanSwapchain::ChooseSwapchainExtent(
        const uint32_t width,
        const uint32_t height) const
    {
        if (m_Capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
            return m_Capabilities.currentExtent;

        VkExtent2D actualExtent = {width, height};

        actualExtent.width = std::clamp(
            actualExtent.width,
            m_Capabilities.minImageExtent.width,
            m_Capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(
            actualExtent.height,
            m_Capabilities.minImageExtent.height,
            m_Capabilities.maxImageExtent.height);

        return actualExtent;
    }

    VkFormat VulkanSwapchain::ChooseDepthFormat() const
    {
        const VAArray candidates = {
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT
        };

        constexpr uint32_t flags = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

        for (auto& candidate : candidates)
        {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(
                m_Device->GetPhysicalDeviceHandle(),
                candidate,
                &props);

            if ((props.linearTilingFeatures & flags) == flags)
            {
                return candidate;
            }
            else if ((props.optimalTilingFeatures & flags) == flags)
            {
                return candidate;
            }
        }

        VA_ENGINE_WARN("[VulkanRHI] Unable to find a suitable depth format.");
        return VK_FORMAT_UNDEFINED;
    }

    std::optional<uint32_t> VulkanSwapchain::AcquireNextImage(
        const uint64_t timeout,
        const VkSemaphore semaphore,
        const VkFence fence) const
    {
        uint32_t imageIndex;
        const auto result = vkAcquireNextImageKHR(
            m_Device->GetLogicalDeviceHandle(),
            m_Swapchain,
            timeout,
            semaphore,
            fence,
            &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            // TODO Recreate the swapchain.
            return {};
        }

        if (VA_VULKAN_CHECK_RESULT(result))
        {
            VA_ENGINE_CRITICAL("[VulkanSwapchain] Failed to acquire next image.");
            throw std::runtime_error("Failed to acquire next image.");
        }

        return {imageIndex};
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

        if (const auto result = vkQueuePresentKHR(graphicsQueue, &presentInfo); result ==
            VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        {
            // TODO Recreate the swapchain
        }
        else if (VA_VULKAN_CHECK_RESULT(result))
        {
            VA_ENGINE_CRITICAL("[VulkanSwapchain] Failed to present.");
            throw std::runtime_error("Failed to present.");
        }
    }

    Resources::RenderTargetHandle VulkanSwapchain::GetColorRenderTarget(const uint32_t index) const
    {
        return m_ColorRenderTargets[index];
    }

    Resources::RenderTargetHandle VulkanSwapchain::GetDepthRenderTarget() const
    {
        return m_DepthRenderTarget;
    }
} // namespace VoidArchitect::Platform
