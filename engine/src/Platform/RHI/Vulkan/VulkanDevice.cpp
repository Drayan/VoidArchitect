//
// Created by Michael Desmedt on 16/05/2025.
//
#include "VulkanDevice.hpp"

#include "Core/Core.hpp"
#include "Core/Logger.hpp"
#include "Core/Window.hpp"
#include "Platform/SDLWindow.hpp"

#include <SDL3/SDL_vulkan.h>

namespace VoidArchitect::Platform
{
    VulkanDevice::VulkanDevice(
        VkInstance instance,
        VkAllocationCallbacks* allocator,
        const std::unique_ptr<Window>& window,
        const DeviceRequirements& requirements)
        : m_Instance(instance),
          m_Allocator(allocator),
          m_PhysicalDevice(VK_NULL_HANDLE),
          m_LogicalDevice(VK_NULL_HANDLE),
          m_Surface{},
          m_PhysicalDeviceProperties{},
          m_PhysicalDeviceFeatures{},
          m_PhysicalDeviceMemoryProperties{},
          m_GraphicsQueue{},
          m_PresentQueue{},
          m_TransferQueue{},
          m_ComputeQueue{}
    {
        SelectPhysicalDevice(requirements);

        //TEMP We should not use SDL inside the VulkanRHI
        if (!SDL_Vulkan_CreateSurface(
            reinterpret_cast<SDLWindow*>(window.get())->GetNativeWindow(),
            m_Instance,
            m_Allocator,
            &m_Surface
        ))
        {
            VA_ENGINE_CRITICAL(
                "[VulkanDevice] Failed to create surface. SDL Error: {}",
                SDL_GetError()
            );
            throw std::runtime_error("Failed to create surface.");
        }
        VA_ENGINE_INFO("[VulkanDevice] Surface created.");

        CreateLogicalDevice(requirements);
    }

    VulkanDevice::~VulkanDevice()
    {
        DestroyLogicalDevice();

        SDL_Vulkan_DestroySurface(m_Instance, m_Surface, m_Allocator);
        VA_ENGINE_INFO("[VulkanDevice] Surface destroyed.");
    }

    void VulkanDevice::SelectPhysicalDevice(const DeviceRequirements& requirements)
    {
        // First, querying for deviceCount
        unsigned int deviceCount = 0;
        vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);

        // Now, querying for device information.
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

        if (deviceCount == 0)
        {
            VA_ENGINE_CRITICAL("[VulkanDevice] No physical devices found.");
            throw std::runtime_error("No physical devices found.");
        }
        VA_ENGINE_DEBUG("[VulkanDevice] Found {} physical devices.", deviceCount);

        for (const auto& device : devices)
        {
            if (device != VK_NULL_HANDLE && IsDeviceMeetRequirements(device, requirements))
            {
                m_PhysicalDevice = device;
                break;
            }
        }

        if (m_PhysicalDevice == VK_NULL_HANDLE)
        {
            VA_ENGINE_CRITICAL("[VulkanDevice] No suitable physical device.");
            throw std::runtime_error("No suitable physical device.");
        }
    }

    bool VulkanDevice::IsDeviceMeetRequirements(
        const VkPhysicalDevice& device,
        const DeviceRequirements& requirements)
    {
        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        if (requirements.DedicatedGPU)
        {
            if (deviceProperties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            {
                VA_ENGINE_DEBUG(
                    "[VulkanDevice] Device is not a discrete GPU, which is a requirement."
                );
                return false;
            }
        }

        VkPhysicalDeviceMemoryProperties memoryProperties;
        vkGetPhysicalDeviceMemoryProperties(device, &memoryProperties);

        // --- Device's extensions ---
        uint32_t extensionCount = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        if (vkEnumerateDeviceExtensionProperties(
            device,
            nullptr,
            &extensionCount,
            availableExtensions.data()
        ))
        {
            VA_ENGINE_CRITICAL("[VulkanDevice] Failed to enumerate device extensions.");
            throw std::runtime_error("Failed to enumerate device extensions.");
        }

        std::set<std::string> requiredExtensions(
            requirements.Extensions.begin(),
            requirements.Extensions.end()
        );
        for (const auto& extension : availableExtensions)
        {
            requiredExtensions.erase(extension.extensionName);
        }

        if (!requiredExtensions.empty())
        {
            VA_ENGINE_CRITICAL("[VulkanDevice] Device does not support all required extensions.");
            throw std::runtime_error("Device does not support all required extensions.");
        }

        // If we reach this point, the device meet all requirement.
        // TODO Add suitable device to a map with scoring for case when multiple devices can meet
        //      the requirement and selecting the best one.
        m_PhysicalDeviceFeatures = deviceFeatures;
        m_PhysicalDeviceProperties = deviceProperties;
        m_PhysicalDeviceMemoryProperties = memoryProperties;

        VA_ENGINE_DEBUG("[VulkanDevice] Selected device: {}.", deviceProperties.deviceName);
        return true;
    }

    void VulkanDevice::CreateLogicalDevice(const DeviceRequirements& requirements)
    {
        FindQueueFamilies();

        constexpr auto queuePriority = 1.f;
        const auto queueCreateInfo = VkDeviceQueueCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueFamilyIndex = m_GraphicsQueueFamilyIndex.value(),
            .queueCount = 1,
            .pQueuePriorities = &queuePriority,
        };

        const auto deviceCreateInfo = VkDeviceCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &queueCreateInfo,
            .enabledLayerCount = 0,
            .ppEnabledLayerNames = nullptr,
            .enabledExtensionCount = static_cast<uint32_t>(requirements.Extensions.size()),
            .ppEnabledExtensionNames = requirements.Extensions.data(),
            .pEnabledFeatures = &m_PhysicalDeviceFeatures,
        };
        if (vkCreateDevice(m_PhysicalDevice, &deviceCreateInfo, m_Allocator, &m_LogicalDevice) !=
            VK_SUCCESS)
        {
            VA_ENGINE_CRITICAL("[VulkanDevice] Failed to create logical device.");
            throw std::runtime_error("Failed to create logical device.");
        }
        VA_ENGINE_DEBUG("[VulkanDevice] Logical device created.");

        // Now we can retrieve the device's queues
        vkGetDeviceQueue(m_LogicalDevice, m_GraphicsQueueFamilyIndex.value(), 0, &m_GraphicsQueue);
        vkGetDeviceQueue(m_LogicalDevice, m_PresentQueueFamilyIndex.value(), 0, &m_PresentQueue);
        vkGetDeviceQueue(m_LogicalDevice, m_TransferQueueFamilyIndex.value(), 0, &m_TransferQueue);
        vkGetDeviceQueue(m_LogicalDevice, m_ComputeQueueFamilyIndex.value(), 0, &m_ComputeQueue);

        // Create command pool for graphics queue
        auto poolCreateInfo = VkCommandPoolCreateInfo{};
        poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolCreateInfo.queueFamilyIndex = m_GraphicsQueueFamilyIndex.value();
        poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        if (vkCreateCommandPool(
                m_LogicalDevice,
                &poolCreateInfo,
                m_Allocator,
                &m_GraphicsCommandPool) !=
            VK_SUCCESS)
        {
            VA_ENGINE_CRITICAL("[VulkanDevice] Failed to create graphics command pool.");
            throw std::runtime_error("Failed to create graphics command pool.");
        }
    }

    void VulkanDevice::DestroyLogicalDevice()
    {
        if (m_GraphicsCommandPool != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(m_LogicalDevice, m_GraphicsCommandPool, m_Allocator);
            m_GraphicsCommandPool = VK_NULL_HANDLE;
            VA_ENGINE_DEBUG("[VulkanDevice] Graphics command pool destroyed.");
        }
        if (m_LogicalDevice != VK_NULL_HANDLE)
        {
            vkDestroyDevice(m_LogicalDevice, m_Allocator);
            m_LogicalDevice = VK_NULL_HANDLE;
            VA_ENGINE_DEBUG("[VulkanDevice] Logical device destroyed.");
        }
    }

    void VulkanDevice::FindQueueFamilies()
    {
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(
            m_PhysicalDevice,
            &queueFamilyCount,
            queueFamilies.data()
        );

        int32_t queueFamilyIndex = 0;
        for (const auto& queueFamily : queueFamilies)
        {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
                m_GraphicsQueueFamilyIndex = queueFamilyIndex;
            if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT)
                m_TransferQueueFamilyIndex = queueFamilyIndex;
            if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
                m_ComputeQueueFamilyIndex = queueFamilyIndex;

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(
                m_PhysicalDevice,
                queueFamilyIndex,
                m_Surface,
                &presentSupport
            );

            if (presentSupport)
                m_PresentQueueFamilyIndex = queueFamilyIndex;

            if (m_GraphicsQueueFamilyIndex.has_value() && m_TransferQueueFamilyIndex.has_value() &&
                m_ComputeQueueFamilyIndex.has_value() && presentSupport)
                break;

            queueFamilyIndex++;
        }

        VA_ENGINE_DEBUG("[VulkanDevice] Found {} queue families.", queueFamilyCount);
        VA_ENGINE_DEBUG(
            "[VulkanDevice] Graphic queue index : {}.",
            m_GraphicsQueueFamilyIndex.value()
        );
        VA_ENGINE_DEBUG(
            "[VulkanDevice] Present queue index : {}.",
            m_PresentQueueFamilyIndex.value()
        );
        VA_ENGINE_DEBUG(
            "[VulkanDevice] Transfer queue index: {}.",
            m_TransferQueueFamilyIndex.value()
        );
        VA_ENGINE_DEBUG(
            "[VulkanDevice] Compute queue index : {}.",
            m_ComputeQueueFamilyIndex.value()
        );
    }
} // VoidArchitect::Platform
