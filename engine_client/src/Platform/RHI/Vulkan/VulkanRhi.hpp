//
// Created by Michael Desmedt on 14/05/2025.
//
#pragma once
#include "Platform/RHI/IRenderingHardware.hpp"

#include "VulkanDevice.hpp"

#include <vulkan/vulkan.h>

namespace VoidArchitect::Platform
{
    class VulkanSwapchain;
}

namespace VoidArchitect
{
    class Window;
}

namespace VoidArchitect::Platform
{
    class VulkanRHI : public IRenderingHardware
    {
    public:
        explicit VulkanRHI(std::unique_ptr<Window>& window);
        ~VulkanRHI() override;

        [[nodiscard]] VkSurfaceCapabilitiesKHR GetSwapchainCapabilities() const
        {
            return m_Capabilities;
        }

    private:
        void CreateInstance();
        void CreateDevice(std::unique_ptr<Window>& window);
        void CreateSwapchain();

        void QuerySwapchainCapabilities();

        [[nodiscard]] VkSurfaceFormatKHR ChooseSwapchainFormat() const;
        [[nodiscard]] VkPresentModeKHR ChooseSwapchainPresentMode() const;
        [[nodiscard]] VkExtent2D ChooseSwapchainExtent() const;

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

        std::unique_ptr<VulkanSwapchain> m_Swapchain;
    };
} // VoidArchitect
