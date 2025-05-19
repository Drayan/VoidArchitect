//
// Created by Michael Desmedt on 16/05/2025.
//
#pragma once
#include <vulkan/vulkan_core.h>

namespace VoidArchitect
{
    class Window;
}

namespace VoidArchitect::Platform
{
    struct DeviceRequirements
    {
        bool DedicatedGPU = false;
        std::vector<const char*> Extensions;
    };

    class VulkanDevice
    {
    public:
        VulkanDevice(
            VkInstance instance,
            VkAllocationCallbacks* allocator,
            const std::unique_ptr<Window>& window,
            const DeviceRequirements& requirements);
        ~VulkanDevice();

        [[nodiscard]] VkDevice GetLogicalDeviceHandle() const { return m_LogicalDevice; }
        [[nodiscard]] VkPhysicalDevice GetPhysicalDeviceHandle() const { return m_PhysicalDevice; }
        VkSurfaceKHR& GetRefSurface() { return m_Surface; }

        [[nodiscard]] std::optional<uint32_t> GetGraphicsFamily() const
        {
            return m_GraphicsQueueFamilyIndex;
        }

        [[nodiscard]] std::optional<uint32_t> GetPresentFamily() const
        {
            return m_PresentQueueFamilyIndex;
        }

        [[nodiscard]] std::optional<uint32_t> GetTransferFamily() const
        {
            return m_TransferQueueFamilyIndex;
        }

        [[nodiscard]] std::optional<uint32_t> GetComputeFamily() const
        {
            return m_ComputeQueueFamilyIndex;
        }

    private:
        void SelectPhysicalDevice(const DeviceRequirements& requirements);
        bool IsDeviceMeetRequirements(
            const VkPhysicalDevice& device,
            const DeviceRequirements& requirements);

        void CreateLogicalDevice(const DeviceRequirements& requirements);
        void DestroyLogicalDevice();

        void FindQueueFamilies();

        VkInstance m_Instance;
        VkAllocationCallbacks* m_Allocator;
        VkPhysicalDevice m_PhysicalDevice;
        VkDevice m_LogicalDevice;
        VkSurfaceKHR m_Surface;

        // --- Physical device properties ---
        VkPhysicalDeviceProperties m_PhysicalDeviceProperties;
        VkPhysicalDeviceFeatures m_PhysicalDeviceFeatures;
        VkPhysicalDeviceMemoryProperties m_PhysicalDeviceMemoryProperties;

        std::optional<uint32_t> m_GraphicsQueueFamilyIndex;
        std::optional<uint32_t> m_PresentQueueFamilyIndex;
        std::optional<uint32_t> m_TransferQueueFamilyIndex;
        std::optional<uint32_t> m_ComputeQueueFamilyIndex;

        VkQueue m_GraphicsQueue;
        VkQueue m_PresentQueue;
        VkQueue m_TransferQueue;
        VkQueue m_ComputeQueue;
    };
} // VoidArchitect::Platform
