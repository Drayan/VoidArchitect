//
// Created by Michael Desmedt on 14/05/2025.
//
#include "VulkanRhi.hpp"

#include "Core/Logger.hpp"
#include "Core/Window.hpp"

#include <SDL3/SDL_vulkan.h>

#include "VulkanPipeline.hpp"
#include "VulkanRenderpass.hpp"
#include "VulkanSwapchain.hpp"

namespace VoidArchitect::Platform
{
#ifdef DEBUG
    VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebuggerCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT*
        pCallbackData,
        void* pUserData);
#endif

    VulkanRHI::VulkanRHI(std::unique_ptr<Window>& window)
        : m_DebugMessenger{},
          m_Window(window),
          m_Allocator(nullptr),
          m_Instance{},
          m_Capabilities{}
    {
        //NOTE Currently we don't provide an allocator, but we might want to do it in the future
        // that's why there's m_Allocator already.
        m_Allocator = nullptr;

        CreateInstance();
        CreateDevice(window);
        CreateSwapchain();
        CreateRenderpass();

        CreatePipeline();
    }

    VulkanRHI::~VulkanRHI()
    {
        m_Pipeline.reset();
        VA_ENGINE_INFO("[VulkanRHI] Pipeline destroyed.");

        m_MainRenderpass.reset();
        VA_ENGINE_INFO("[VulkanRHI] Main renderpass destroyed.");

        m_Swapchain.reset();
        VA_ENGINE_INFO("[VulkanRHI] Swapchain destroyed.");

        m_Device.reset();
        VA_ENGINE_INFO("[VulkanRHI] Device destroyed.");

#ifdef DEBUG
        DestroyDebugMessenger();
#endif
        if (m_Instance != VK_NULL_HANDLE)
            vkDestroyInstance(m_Instance, m_Allocator);
        VA_ENGINE_INFO("[VulkanRHI] Instance destroyed.");
    }

    int32_t VulkanRHI::FindMemoryIndex(
        const uint32_t typeFilter,
        const uint32_t propertyFlags) const
    {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(
            m_Device->GetPhysicalDeviceHandle(),
            &memProperties
        );

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        {
            if ((typeFilter & (1 << i)) &&
                (memProperties.memoryTypes[i].propertyFlags & propertyFlags) == propertyFlags)
            {
                return i;
            }
        }

        VA_ENGINE_WARN("[VulkanRHI] Cannot find memory index.");
        return -1;
    }

    void VulkanRHI::CreateInstance()
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
        std::vector<const char*> requiredValidationLayers;
#ifdef DEBUG
        VA_ENGINE_DEBUG("[VulkanRHI] Validation layers enabled.");

        requiredValidationLayers.push_back("VK_LAYER_KHRONOS_validation");

        // Get a list of supported validation layers
        uint32_t layerCount;
        if (vkEnumerateInstanceLayerProperties(&layerCount, nullptr) != VK_SUCCESS)
        {
            VA_ENGINE_CRITICAL("[VulkanRHI] Failed to enumerate instance layers.");
            return;
        }
        std::vector<VkLayerProperties> availableLayers(layerCount);
        if (vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data()) != VK_SUCCESS)
        {
            VA_ENGINE_CRITICAL("[VulkanRHI] Failed to enumerate instance layers.");
            return;
        }

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
                    layerName
                );
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
        if (vkCreateInstance(&instanceCreateInfo, m_Allocator, &m_Instance) != VK_SUCCESS)
        {
            std::stringstream ss;
            for (unsigned int i = 0; i < extensionCount; ++i)
            {
                ss << extensions[i] << std::endl;
            }
            VA_ENGINE_CRITICAL(
                "[VulkanRHI] Failed to initialize instance.\nRequired extensions:\n%{}",
                ss.str()
            );

#ifdef DEBUG
            CleaningDebugExtensionsArray(extensions, extensionCount);
            extensions = nullptr;
#endif
            return;
        }
        VA_ENGINE_INFO("[VulkanRHI] Instance initialized.");

#ifdef DEBUG
        CleaningDebugExtensionsArray(extensions, extensionCount);

        CreateDebugMessenger();
#endif
    }

    void VulkanRHI::CreateDevice(std::unique_ptr<Window>& window)
    {
        //TODO DeviceRequirements should be configurable by the Application, but for now we will
        // keep it simple and make it directly here.
        DeviceRequirements requirements;
        // NOTE With Mx chips from Apple, we should add the Portability extension, there's probably
        //  other extensions needed for other GPUs, so for now I will just add them here for my GPU
        //  but we need a more robust solution.
        requirements.Extensions = {
            // Swapchain is needed for graphics
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            // Portability subset must be enabled on Mx chips.
            "VK_KHR_portability_subset",
        };
        m_Device = std::make_unique<VulkanDevice>(m_Instance, m_Allocator, window, requirements);
        VA_ENGINE_INFO("[VulkanRHI] Device created.");
    }

    void VulkanRHI::CreateSwapchain()
    {
        QuerySwapchainCapabilities();
        VA_ENGINE_DEBUG("[VulkanRHI] Swapchain capabilities queried.");
        auto extents = ChooseSwapchainExtent();
        auto format = ChooseSwapchainFormat();
        auto mode = ChooseSwapchainPresentMode();
        auto depthFormat = ChooseDepthFormat();
        VA_ENGINE_DEBUG(
            "[VulkanRHI] Swapchain format, depth format, present mode and extent chosen.");

        m_Swapchain = std::make_unique<VulkanSwapchain>(
            *this,
            m_Device,
            m_Allocator,
            format,
            mode,
            extents,
            depthFormat);
        VA_ENGINE_INFO("[VulkanRHI] Swapchain created.");
    }

    void VulkanRHI::QuerySwapchainCapabilities()
    {
        const auto physicalDevice = m_Device->GetPhysicalDeviceHandle();
        const auto surface = m_Device->GetRefSurface();
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            physicalDevice,
            surface,
            &m_Capabilities
        );

        // --- Formats ---
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            physicalDevice,
            surface,
            &formatCount,
            nullptr
        );

        if (formatCount != 0)
        {
            m_Formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(
                physicalDevice,
                surface,
                &formatCount,
                m_Formats.data()
            );
        }
        else
        {
            VA_ENGINE_ERROR("[VulkanRHI] Failed to query surface formats.");
        }

        // --- Present modes ---
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            physicalDevice,
            surface,
            &presentModeCount,
            nullptr
        );

        if (presentModeCount != 0)
        {
            m_PresentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(
                physicalDevice,
                surface,
                &presentModeCount,
                m_PresentModes.data()
            );
        }
        else
        {
            VA_ENGINE_ERROR("[VulkanRHI] Failed to query surface present modes.");
        }
    }

    VkSurfaceFormatKHR VulkanRHI::ChooseSwapchainFormat() const
    {
        for (const auto& availableFormat : m_Formats)
        {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
                availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                return availableFormat;
            }
        }

        return m_Formats[0]; // If a required format is not found, we will use the first one.
    }

    VkPresentModeKHR VulkanRHI::ChooseSwapchainPresentMode() const
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

    VkExtent2D VulkanRHI::ChooseSwapchainExtent() const
    {
        if (m_Capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            return m_Capabilities.currentExtent;
        }
        else
        {
            const uint32_t height = m_Window->GetHeight();
            const uint32_t width = m_Window->GetWidth();

            VkExtent2D actualExtent = {
                width,
                height
            };

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
    }

    VkFormat VulkanRHI::ChooseDepthFormat() const
    {
        const std::vector candidates = {
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
                &props
            );

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

    void VulkanRHI::CreateRenderpass()
    {
        m_MainRenderpass = std::make_unique<VulkanRenderpass>(
            m_Device,
            m_Swapchain,
            m_Allocator,
            0,
            0,
            m_FramebufferWidth,
            m_FramebufferHeight,
            0.2f,
            0.0f,
            0.0f,
            1.0f,
            1.0f,
            0
        );
    }

    void VulkanRHI::CreatePipeline()
    {
        m_Pipeline = std::make_unique<VulkanPipeline>(m_Device, m_Allocator);
        VA_ENGINE_INFO("[VulkanRHI] Pipeline created.");
    }

#ifdef DEBUG
    void VulkanRHI::AddDebugExtensions(char const* const*& extensions, unsigned int& extensionCount)
    {
        // In debug build, we want to add an extra extension, but SDL return us an already allocated
        // array, so we must alloc a new, bigger array, and copy the extensions array into, with
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
            debugExtensions[extensionCount]
        );
        extensionCount++;

        VA_ENGINE_DEBUG("[VulkanRHI] Instance extensions: ");
        for (unsigned int i = 0; i < extensionCount; i++)
            VA_ENGINE_DEBUG("\t{}", debugExtensions[i]);

        // Replace the used array with our own.
        extensions = debugExtensions;
    }

    void VulkanRHI::CleaningDebugExtensionsArray(
        char const* const*& extensions,
        const unsigned int extensionCount)
    {
        for (unsigned int i = 0; i < extensionCount; i++)
            delete[] extensions[i];

        delete extensions;
        extensions = nullptr;
    }

    void VulkanRHI::CreateDebugMessenger()
    {
        constexpr unsigned int logSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;

        constexpr unsigned int messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;

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
            reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(
                m_Instance,
                "vkCreateDebugUtilsMessengerEXT"
            ));
        VA_ENGINE_ASSERT(
            vkCreateDebugUtilsMessengerEXT != nullptr,
            "[VulkanRHI] Failed to load debug messenger create function."
        )
        if (vkCreateDebugUtilsMessengerEXT(
            m_Instance,
            &debugCreateInfo,
            m_Allocator,
            &m_DebugMessenger
        ) != VK_SUCCESS)
        {
            VA_ENGINE_CRITICAL("[VulkanRHI] Failed to create debug messenger.");
            return;
        }

        VA_ENGINE_INFO("[VulkanRHI] Debug messenger created.");
    }

    void VulkanRHI::DestroyDebugMessenger() const
    {
        if (m_DebugMessenger != VK_NULL_HANDLE)
        {
            const auto vkDestroyDebugUtilsMessengerEXT =
                reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(
                    m_Instance,
                    "vkDestroyDebugUtilsMessengerEXT"
                ));
            VA_ENGINE_ASSERT(
                vkDestroyDebugUtilsMessengerEXT != nullptr,
                "[VulkanRHI] Failed to load debug messenger destroy function."
            )
            vkDestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, m_Allocator);
            VA_ENGINE_INFO("[VulkanRHI] Debug messenger destroyed.");
        }
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebuggerCallback(
        const VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData)
    {
        if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        {
            VA_ENGINE_ERROR("[VulkanRHI::Vulkan] {}", pCallbackData->pMessage);
        }
        else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        {
            VA_ENGINE_WARN("[VulkanRHI::Vulkan] {}", pCallbackData->pMessage);
        }
        else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
        {
            VA_ENGINE_INFO("[VulkanRHI::Vulkan] {}", pCallbackData->pMessage);
        }
        else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
        {
            VA_ENGINE_TRACE("[VulkanRHI::Vulkan] {}", pCallbackData->pMessage);
        }

        return VK_FALSE;
    }
#endif
} // VoidArchitect
