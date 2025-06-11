//
// Created by Michael Desmedt on 17/05/2025.
//
#pragma once

#include "VulkanDevice.hpp"
#include "VulkanImage.hpp"

#include <vulkan/vulkan.h>

namespace VoidArchitect::Platform
{
    class VulkanSwapchain
    {
    public:
        VulkanSwapchain(
            const std::unique_ptr<VulkanDevice>& device,
            VkAllocationCallbacks* allocator,
            uint32_t width,
            uint32_t height);
        ~VulkanSwapchain();

        void Recreate(uint32_t width, uint32_t height);

        std::optional<uint32_t> AcquireNextImage(
            uint64_t timeout,
            VkSemaphore semaphore,
            VkFence fence) const;
        void Present(VkQueue graphicsQueue, VkSemaphore renderComplete, uint32_t imageIndex) const;

        Resources::RenderTargetHandle GetColorRenderTarget(uint32_t index) const;
        Resources::RenderTargetHandle GetDepthRenderTarget() const;
        uint32_t GetImageCount() const { return m_SwapchainImages.size(); }
        VkFormat GetFormat() const { return m_Format.format; };
        VkFormat GetDepthFormat() const { return m_DepthFormat; };
        uint32_t GetMaxFrameInFlight() const { return m_MaxFrameInFlight; };

    private:
        void Cleanup();
        void Create(uint32_t width, uint32_t height);
        void CreateRenderTargetViews();

        void QuerySwapchainCapabilities();
        [[nodiscard]] VkSurfaceFormatKHR ChooseSwapchainFormat() const;
        [[nodiscard]] VkPresentModeKHR ChooseSwapchainPresentMode() const;
        [[nodiscard]] VkExtent2D ChooseSwapchainExtent(uint32_t width, uint32_t height) const;
        [[nodiscard]] VkFormat ChooseDepthFormat() const;

        const std::unique_ptr<VulkanDevice>& m_Device;
        VkAllocationCallbacks* m_Allocator;
        VkSwapchainKHR m_Swapchain;

        VkSurfaceCapabilitiesKHR m_Capabilities;
        VAArray<VkSurfaceFormatKHR> m_Formats;
        VAArray<VkPresentModeKHR> m_PresentModes;

        VkSurfaceFormatKHR m_Format;
        VkPresentModeKHR m_PresentMode;
        VkExtent2D m_Extent;
        VkFormat m_DepthFormat;

        VAArray<Resources::RenderTargetHandle> m_ColorRenderTargets;
        Resources::RenderTargetHandle m_DepthRenderTarget;

        VAArray<VkImage> m_SwapchainImages;
        uint32_t m_MaxFrameInFlight = 2; // Support triple-buffering by default
    };
} // namespace VoidArchitect::Platform
