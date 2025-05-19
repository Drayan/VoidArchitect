//
// Created by Michael Desmedt on 14/05/2025.
//
#pragma once
#include "Platform/RHI/IRenderingHardware.hpp"

#include "VulkanDevice.hpp"

#include <vulkan/vulkan.h>

namespace VoidArchitect
{
    class Window;
}

namespace VoidArchitect::Platform
{
    class VulkanCommandBuffer;
    class VulkanSwapchain;
    class VulkanRenderpass;
    class VulkanPipeline;

    class VulkanRHI : public IRenderingHardware
    {
    public:
        explicit VulkanRHI(std::unique_ptr<Window>& window);
        ~VulkanRHI() override;

        [[nodiscard]] VkSurfaceCapabilitiesKHR GetSwapchainCapabilities() const
        {
            return m_Capabilities;
        }

        void SetImageIndex(const uint32_t index) { m_ImageIndex = index; }
        void SetCurrentIndex(const uint32_t index) { m_CurrentIndex = index; }

        int32_t FindMemoryIndex(uint32_t typeFilter, uint32_t propertyFlags) const;

    private:
        void CreateInstance();
        void CreateDevice(std::unique_ptr<Window>& window);
        void CreateSwapchain();

        void QuerySwapchainCapabilities();

        [[nodiscard]] VkSurfaceFormatKHR ChooseSwapchainFormat() const;
        [[nodiscard]] VkPresentModeKHR ChooseSwapchainPresentMode() const;
        [[nodiscard]] VkExtent2D ChooseSwapchainExtent() const;
        VkFormat ChooseDepthFormat() const;

        void CreateRenderpass();
        void CreateCommandBuffers();
        void CreatePipeline();

#ifdef DEBUG
        void CreateDebugMessenger();
        void DestroyDebugMessenger() const;
        static void AddDebugExtensions(
            char const* const*& extensions,
            unsigned int& extensionCount);
        static void CleaningDebugExtensionsArray(
            char const* const*& extensions,
            unsigned int extensionCount);

        VkDebugUtilsMessengerEXT m_DebugMessenger;
#endif

        std::unique_ptr<Window>& m_Window;

        VkAllocationCallbacks* m_Allocator;
        VkInstance m_Instance;

        std::unique_ptr<VulkanDevice> m_Device;

        VkSurfaceCapabilitiesKHR m_Capabilities;
        std::vector<VkSurfaceFormatKHR> m_Formats;
        std::vector<VkPresentModeKHR> m_PresentModes;

        uint32_t m_ImageIndex;
        uint32_t m_CurrentIndex;
        std::unique_ptr<VulkanSwapchain> m_Swapchain;
        std::unique_ptr<VulkanRenderpass> m_MainRenderpass;
        std::vector<std::unique_ptr<VulkanCommandBuffer>> m_GraphicsCommandBuffers;

        std::unique_ptr<VulkanPipeline> m_Pipeline;

        uint32_t m_FramebufferWidth;
        uint32_t m_FramebufferHeight;
    };
} // VoidArchitect
