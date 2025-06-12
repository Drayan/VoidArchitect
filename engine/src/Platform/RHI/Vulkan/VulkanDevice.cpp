//
// Created by Michael Desmedt on 16/05/2025.
//
#include "VulkanDevice.hpp"

#include "Core/Core.hpp"
#include "Core/Logger.hpp"
#include "Core/Window.hpp"
#include "Platform/SDLWindow.hpp"

#include <SDL3/SDL_vulkan.h>

#include "VulkanUtils.hpp"

namespace VoidArchitect::Platform
{
#ifdef DEBUG
    VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebuggerCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);
#endif

    VulkanDevice::VulkanDevice(
        VkAllocationCallbacks* allocator,
        const std::unique_ptr<Window>& window,
        const DeviceRequirements& requirements)
        : m_Allocator(allocator),
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
        CreateInstance();
        SelectPhysicalDevice(requirements);

        // TEMP We should not use SDL inside the VulkanRHI
        if (!SDL_Vulkan_CreateSurface(
            reinterpret_cast<SDLWindow*>(window.get())->GetNativeWindow(),
            m_Instance,
            m_Allocator,
            &m_Surface))
        {
            VA_ENGINE_CRITICAL(
                "[VulkanDevice] Failed to create surface. SDL Error: {}",
                SDL_GetError());
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

#ifdef DEBUG
        DestroyDebugMessenger();
#endif
        if (m_Instance != VK_NULL_HANDLE)
            vkDestroyInstance(m_Instance, m_Allocator);
        VA_ENGINE_INFO("[VulkanDevice] Instance destroyed.");
    }

    void VulkanDevice::WaitIdle() const
    {
        VA_VULKAN_CHECK_RESULT_WARN(vkDeviceWaitIdle(m_LogicalDevice));
        VA_ENGINE_DEBUG("[VulkanDevice] Device wait idle.");
    }

    void VulkanDevice::CreateInstance()
    {
        constexpr auto appInfo = VkApplicationInfo{
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pNext = nullptr,
            .pApplicationName = "Void Architect",
            .applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0),
            .pEngineName = "Void Architect Engine",
            .engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0),
            .apiVersion = VK_API_VERSION_1_2,
        };

        unsigned int extensionCount = 0;
        auto extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);

        VA_ENGINE_ASSERT(extensionCount > 0, "[VulkanRHI] No Vulkan extensions found.");

#ifdef DEBUG
        AddDebugExtensions(extensions, extensionCount);
#endif

        // Validation layers
        VAArray<const char*> requiredValidationLayers;
#if defined(DEBUG) || defined(FORCE_VALIDATION)
        VA_ENGINE_DEBUG("[VulkanRHI] Validation layers enabled.");

        requiredValidationLayers.push_back("VK_LAYER_KHRONOS_validation");
        // FIXME: In case of things going extremely wrong, enable this layer to see every call.
        // requiredValidationLayers.push_back("VK_LAYER_LUNARG_api_dump");

        // Get a list of supported validation layers
        uint32_t layerCount;
        VA_VULKAN_CHECK_RESULT_CRITICAL(vkEnumerateInstanceLayerProperties(&layerCount, nullptr));
        VAArray<VkLayerProperties> availableLayers(layerCount);
        VA_VULKAN_CHECK_RESULT_CRITICAL(
            vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data()));

        // Verify that the required validation layers are supported
        for (const char* layerName : requiredValidationLayers)
        {
            VA_ENGINE_DEBUG("[VulkanRHI] Checking layer: {}", layerName);
            auto layerFound = false;
            for (const auto& layerProperties : availableLayers)
            {
                if (strcmp(layerName, layerProperties.layerName) == 0)
                {
                    layerFound = true;
                    VA_ENGINE_DEBUG("Found.");
                    break;
                }
            }

            if (!layerFound)
            {
                VA_ENGINE_CRITICAL(
                    "[VulkanRHI] Required validation layer {} is not supported.",
                    layerName);
                return;
            }
        }

        VA_ENGINE_DEBUG("[VulkanRHI] All required validation layers are supported.");
#endif

        VkInstanceCreateFlags flags = 0;
#ifdef VOID_ARCH_PLATFORM_MACOS
        flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

        const auto instanceCreateInfo = VkInstanceCreateInfo{
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = flags,
            .pApplicationInfo = &appInfo,
            .enabledLayerCount = static_cast<uint32_t>(requiredValidationLayers.size()),
            .ppEnabledLayerNames = requiredValidationLayers.data(),
            .enabledExtensionCount = extensionCount,
            .ppEnabledExtensionNames = extensions,
        };
        if (VA_VULKAN_CHECK_RESULT(vkCreateInstance(&instanceCreateInfo, m_Allocator, &m_Instance)))
        {
            std::stringstream ss;
            for (unsigned int i = 0; i < extensionCount; ++i)
                ss << extensions[i] << std::endl;

            VA_ENGINE_CRITICAL(
                "[VulkanRHI] Failed to initialize instance.\nRequired extensions:\n%{}",
                ss.str());

#ifdef DEBUG
            CleaningDebugExtensionsArray(extensions, extensionCount);
#endif
            return;
        }
        VA_ENGINE_INFO("[VulkanRHI] Instance initialized.");

#ifdef DEBUG
        CleaningDebugExtensionsArray(extensions, extensionCount);

        CreateDebugMessenger();
#endif
    }

    void VulkanDevice::SelectPhysicalDevice(const DeviceRequirements& requirements)
    {
        // First, querying for deviceCount
        unsigned int deviceCount = 0;
        VA_VULKAN_CHECK_RESULT_CRITICAL(
            vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr));

        // Now, querying for device information.
        VAArray<VkPhysicalDevice> devices(deviceCount);
        VA_VULKAN_CHECK_RESULT_CRITICAL(
            vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data()));

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
                    "[VulkanDevice] Device is not a discrete GPU, which is a requirement.");
                return false;
            }
        }

        VkPhysicalDeviceMemoryProperties memoryProperties;
        vkGetPhysicalDeviceMemoryProperties(device, &memoryProperties);

        // --- Device's extensions ---
        uint32_t extensionCount = 0;
        VA_VULKAN_CHECK_RESULT_CRITICAL(
            vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr));
        VAArray<VkExtensionProperties> availableExtensions(extensionCount);
        if (VA_VULKAN_CHECK_RESULT(
            vkEnumerateDeviceExtensionProperties(
                device, nullptr, &extensionCount, availableExtensions.data())))
        {
            VA_ENGINE_CRITICAL("[VulkanDevice] Failed to enumerate device extensions.");
            throw std::runtime_error("Failed to enumerate device extensions.");
        }

        std::set<std::string> requiredExtensions(
            requirements.Extensions.begin(),
            requirements.Extensions.end());
        for (const auto& extension : availableExtensions)
            requiredExtensions.erase(extension.extensionName);

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
        if (VA_VULKAN_CHECK_RESULT(
            vkCreateDevice(m_PhysicalDevice, &deviceCreateInfo, m_Allocator, &m_LogicalDevice)))
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
        if (VA_VULKAN_CHECK_RESULT(
            vkCreateCommandPool(
                m_LogicalDevice, &poolCreateInfo, m_Allocator, &m_GraphicsCommandPool)))
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

        VAArray<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(
            m_PhysicalDevice,
            &queueFamilyCount,
            queueFamilies.data());

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
            VA_VULKAN_CHECK_RESULT_WARN(
                vkGetPhysicalDeviceSurfaceSupportKHR(
                    m_PhysicalDevice, queueFamilyIndex, m_Surface, &presentSupport));

            if (presentSupport)
                m_PresentQueueFamilyIndex = queueFamilyIndex;

            if (m_GraphicsQueueFamilyIndex.has_value() && m_TransferQueueFamilyIndex.has_value()
                && m_ComputeQueueFamilyIndex.has_value() && presentSupport)
                break;

            queueFamilyIndex++;
        }

        VA_ENGINE_DEBUG("[VulkanDevice] Found {} queue families.", queueFamilyCount);
        VA_ENGINE_DEBUG(
            "[VulkanDevice] Graphic queue index : {}.",
            m_GraphicsQueueFamilyIndex.value());
        VA_ENGINE_DEBUG(
            "[VulkanDevice] Present queue index : {}.",
            m_PresentQueueFamilyIndex.value());
        VA_ENGINE_DEBUG(
            "[VulkanDevice] Transfer queue index: {}.",
            m_TransferQueueFamilyIndex.value());
        VA_ENGINE_DEBUG(
            "[VulkanDevice] Compute queue index : {}.",
            m_ComputeQueueFamilyIndex.value());
    }

    int32_t VulkanDevice::FindMemoryIndex(
        const uint32_t typeFilter,
        const uint32_t propertyFlags) const
    {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        {
            if ((typeFilter & (1 << i))
                && (memProperties.memoryTypes[i].propertyFlags & propertyFlags) == propertyFlags)
            {
                return static_cast<int32_t>(i);
            }
        }

        VA_ENGINE_WARN("[VulkanRHI] Cannot find memory index.");
        return -1;
    }

#ifdef DEBUG
    void VulkanDevice::AddDebugExtensions(
        char const* const*& extensions,
        unsigned int& extensionCount)
    {
        // In debug build, we want to add an extra extension, but SDL return us an already allocated
        // array, so we must alloc a new, bigger array, and copy the extension array into, with
        // any new extensions needed.
        const auto debugExtensions = new char*[extensionCount + 1];
        // Copying the SDL array
        for (unsigned int i = 0; i < extensionCount; i++)
        {
            const size_t len = strlen(extensions[i]) + 1; // +1 for null terminator
            debugExtensions[i] = new char[len];
            std::copy_n(extensions[i], strlen(extensions[i]), debugExtensions[i]);
            debugExtensions[i][len - 1] = '\0'; // Ensure null termination
        }
        // Adding the VK_EXT_DEBUG_UTILS_EXTENSION_NAME extension
        const size_t debugExtLen = strlen(VK_EXT_DEBUG_UTILS_EXTENSION_NAME) + 1;
        // +1 for null terminator
        debugExtensions[extensionCount] = new char[debugExtLen];
        std::copy_n(
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
            strlen(VK_EXT_DEBUG_UTILS_EXTENSION_NAME),
            debugExtensions[extensionCount]);
        extensionCount++;

        VA_ENGINE_DEBUG("[VulkanRHI] Instance extensions: ");
        for (unsigned int i = 0; i < extensionCount; i++)
            VA_ENGINE_DEBUG("\t{}", debugExtensions[i]);

        // Replace the used array with our own.
        extensions = debugExtensions;
    }

    void VulkanDevice::CleaningDebugExtensionsArray(
        char const* const*& extensions,
        const unsigned int extensionCount)
    {
        for (unsigned int i = 0; i < extensionCount; i++)
            delete[] extensions[i];

        delete extensions;
        extensions = nullptr;
    }

    void VulkanDevice::CreateDebugMessenger()
    {
        constexpr unsigned int logSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;

        constexpr unsigned int messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;

        constexpr auto debugCreateInfo = VkDebugUtilsMessengerCreateInfoEXT{
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .pNext = nullptr,
            .flags = 0,
            .messageSeverity = logSeverity,
            .messageType = messageType,
            .pfnUserCallback = VulkanDebuggerCallback,
            .pUserData = nullptr
        };
        const auto vkCreateDebugUtilsMessengerEXT =
            reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(m_Instance, "vkCreateDebugUtilsMessengerEXT"));
        VA_ENGINE_ASSERT(
            vkCreateDebugUtilsMessengerEXT != nullptr,
            "[VulkanRHI] Failed to load debug messenger create function.")
        VA_VULKAN_CHECK_RESULT_CRITICAL(
            vkCreateDebugUtilsMessengerEXT(
                m_Instance, &debugCreateInfo, m_Allocator, &m_DebugMessenger));

        VA_ENGINE_INFO("[VulkanRHI] Debug messenger created.");
    }

    void VulkanDevice::DestroyDebugMessenger() const
    {
        if (m_DebugMessenger != VK_NULL_HANDLE)
        {
            const auto vkDestroyDebugUtilsMessengerEXT =
                reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                    vkGetInstanceProcAddr(m_Instance, "vkDestroyDebugUtilsMessengerEXT"));
            VA_ENGINE_ASSERT(
                vkDestroyDebugUtilsMessengerEXT != nullptr,
                "[VulkanRHI] Failed to load debug messenger destroy function.")
            vkDestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, m_Allocator);
            VA_ENGINE_INFO("[VulkanRHI] Debug messenger destroyed.");
        }
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebuggerCallback(
        const VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        [[maybe_unused]] void* pUserData)
    {
        if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
            VA_ENGINE_ERROR("[VulkanRHI::Vulkan] {}", pCallbackData->pMessage);
        else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
            VA_ENGINE_WARN("[VulkanRHI::Vulkan] {}", pCallbackData->pMessage);
        else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
            VA_ENGINE_INFO("[VulkanRHI::Vulkan] {}", pCallbackData->pMessage);
        else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
            VA_ENGINE_TRACE("[VulkanRHI::Vulkan] {}", pCallbackData->pMessage);

        return VK_FALSE;
    }
#endif
} // namespace VoidArchitect::Platform
