//
// Created by Michael Desmedt on 14/05/2025.
//
#include "VulkanRhi.hpp"

#include "Core/Logger.hpp"
#include "Core/Window.hpp"

#include <SDL3/SDL_vulkan.h>

#include "VulkanBuffer.hpp"
#include "VulkanDescriptorSetLayoutManager.hpp"
#include "VulkanFence.hpp"
#include "VulkanMaterial.hpp"
#include "VulkanMaterialBufferManager.hpp"
#include "VulkanMesh.hpp"
#include "VulkanPipeline.hpp"
#include "VulkanRenderPass.hpp"
#include "VulkanShader.hpp"
#include "VulkanSwapchain.hpp"
#include "VulkanTexture.hpp"
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

    VulkanRHI::VulkanRHI(
        std::unique_ptr<Window>& window,
        const PipelineInputLayout& sharedInputLayout)
        : m_Window(window),
          m_Allocator(nullptr),
          m_Instance{},
          m_Capabilities{},
          m_FramebufferWidth(window->GetWidth()),
          m_FramebufferHeight(window->GetHeight())
    {
        // NOTE Currently we don't provide an allocator, but we might want to do it in the future
        //  that's why there's m_Allocator already.
        m_Allocator = nullptr;

        CreateInstance();
        CreateDevice(window);
        CreateSwapchain();
        CreateCommandBuffers();
        CreateSyncObjects();

        // Create the DescriptorSetLayoutManager
        g_VkDescriptorSetLayoutManager = std::make_unique<VulkanDescriptorSetLayoutManager>(
            m_Device,
            m_Allocator,
            sharedInputLayout);
        g_VkMaterialBufferManager = std::make_unique<VulkanMaterialBufferManager>(
            *this,
            m_Device,
            m_Allocator);

        CreateGlobalUBO();
    }

    VulkanRHI::~VulkanRHI()
    {
        m_Device->WaitIdle();

        // TEMP Global UBO
        if (m_GlobalDescriptorSets != nullptr)
        {
            delete[] m_GlobalDescriptorSets;
            m_GlobalDescriptorSets = nullptr;
            VA_ENGINE_TRACE("[VulkanRHI] Global descriptor sets destroyed.");
        }
        m_GlobalUniformBuffer.reset();
        VA_ENGINE_TRACE("[VulkanRHI] Global uniform buffer destroyed.");

        if (m_GlobalDescriptorPool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(
                m_Device->GetLogicalDeviceHandle(),
                m_GlobalDescriptorPool,
                m_Allocator);
            m_GlobalDescriptorPool = VK_NULL_HANDLE;
            VA_ENGINE_TRACE("[VulkanRHI] Global descriptor pool destroyed.");
        }
        // TEMP End of temp code

        g_VkMaterialBufferManager = nullptr;
        g_VkDescriptorSetLayoutManager = nullptr;

        DestroySyncObjects();
        VA_ENGINE_INFO("[VulkanRHI] Sync objects destroyed.");

        m_GraphicsCommandBuffers.clear();
        VA_ENGINE_INFO("[VulkanRHI] Command buffers destroyed.");

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

    void VulkanRHI::Resize(uint32_t width, uint32_t height)
    {
        m_CachedFramebufferWidth = width;
        m_CachedFramebufferHeight = height;
        m_FramebufferSizeGeneration++;

        InvalidateMainTargetsFramebuffers();

        VA_ENGINE_DEBUG(
            "[VulkanRHI] Resizing to {}x{}, generation : {}",
            width,
            height,
            m_FramebufferSizeGeneration);
    }

    void VulkanRHI::WaitIdle(uint64_t timeout) { m_Device->WaitIdle(); }

    bool VulkanRHI::BeginFrame(float deltaTime)
    {
        const auto device = m_Device->GetLogicalDeviceHandle();
        if (m_RecreatingSwapchain)
        {
            const auto result = vkDeviceWaitIdle(device);
            if (result != VK_SUCCESS && result != VK_TIMEOUT)
            {
                VA_ENGINE_ERROR("[VulkanRHI] Failed to wait for device idle.");
                return false;
            }
            VA_ENGINE_INFO("[VulkanRHI] Recreating swapchain.");
        }

        // Check if the framebuffer has been resized. If so, a new swapchain must be created.
        if (m_FramebufferSizeGeneration != m_FramebufferSizeLastGeneration)
        {
            m_Device->WaitIdle();

            if (!RecreateSwapchain())
            {
                return false;
            }
            VA_ENGINE_INFO("[VulkanRHI] Recreating swapchain.");
            return false;
        }

        if (!m_InFlightFences[m_CurrentIndex].Wait(std::numeric_limits<uint64_t>::max()))
        {
            VA_ENGINE_WARN("[VulkanRHI] Failed to wait for fence.");
            return false;
        }

        if (!m_Swapchain->AcquireNextImage(
            std::numeric_limits<uint64_t>::max(),
            m_ImageAvailableSemaphores[m_CurrentIndex],
            nullptr,
            m_ImageIndex))
        {
            return false;
        }

        // Begin recording commands.
        auto& cmdBuf = m_GraphicsCommandBuffers[m_ImageIndex];
        cmdBuf.Reset();
        cmdBuf.Begin();

        const VkViewport viewport = {
            .x = 0.0f,
            .y = static_cast<float>(m_FramebufferHeight),
            .width = static_cast<float>(m_FramebufferWidth),
            .height = -static_cast<float>(m_FramebufferHeight),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        const VkRect2D scissor = {
            .offset = {0, 0},
            .extent = {m_FramebufferWidth, m_FramebufferHeight}
        };

        vkCmdSetViewport(cmdBuf.GetHandle(), 0, 1, &viewport);
        vkCmdSetScissor(cmdBuf.GetHandle(), 0, 1, &scissor);

        //m_MainRenderpass->Begin(cmdBuf, m_Swapchain->GetFramebufferHandle(m_ImageIndex));

        g_VkMaterialBufferManager->FlushUpdates();

        return true;
    }

    bool VulkanRHI::EndFrame(float deltaTime)
    {
        auto& cmdBuf = m_GraphicsCommandBuffers[m_ImageIndex];

        //m_MainRenderpass->End(cmdBuf);

        cmdBuf.End();

        if (m_ImagesInFlight[m_ImageIndex] != nullptr)
        {
            m_ImagesInFlight[m_ImageIndex]->Wait(std::numeric_limits<uint64_t>::max());
        }

        // Mark the image fence as in use by this frame.
        m_ImagesInFlight[m_ImageIndex] = &m_InFlightFences[m_CurrentIndex];

        // Reset the fence for use on the next frame.
        m_InFlightFences[m_CurrentIndex].Reset();

        const auto cmdBufHandle = cmdBuf.GetHandle();
        auto submitInfo = VkSubmitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdBufHandle;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &m_QueueCompleteSemaphores[m_CurrentIndex];
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &m_ImageAvailableSemaphores[m_CurrentIndex];

        const std::vector flags = {
            VkPipelineStageFlags{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT}
        };
        submitInfo.pWaitDstStageMask = flags.data();

        if (VA_VULKAN_CHECK_RESULT(
            vkQueueSubmit(
                m_Device->GetGraphicsQueueHandle(),
                1,
                &submitInfo,
                m_InFlightFences[m_CurrentIndex].GetHandle())))
        {
            VA_ENGINE_ERROR("[VulkanRHI] Failed to submit graphics command buffer.");
            return false;
        }
        cmdBuf.SetState(CommandBufferState::Submitted);

        m_Swapchain->Present(
            m_Device->GetGraphicsQueueHandle(),
            m_QueueCompleteSemaphores[m_CurrentIndex],
            m_ImageIndex);

        m_CurrentIndex = (m_CurrentIndex + 1) % m_Swapchain->GetMaxFrameInFlight();

        return true;
    }

    void VulkanRHI::UpdateGlobalState(
        const Resources::PipelinePtr& pipeline,
        const Math::Mat4& projection,
        const Math::Mat4& view)
    {
        const auto globalUniformObject = Resources::GlobalUniformObject{
            .Projection = projection,
            .View = view,
            .Reserved0 = Math::Mat4::Identity(),
            .Reserved1 = Math::Mat4::Identity(),
        };
        auto data = std::vector{globalUniformObject, globalUniformObject, globalUniformObject};
        m_GlobalUniformBuffer->LoadData(data);

        // Update the descriptor set
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_GlobalUniformBuffer->GetHandle();
        bufferInfo.offset = sizeof(Resources::GlobalUniformObject) * m_ImageIndex;
        bufferInfo.range = sizeof(Resources::GlobalUniformObject);

        VkWriteDescriptorSet writeDescriptorSet{};
        writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSet.dstSet = m_GlobalDescriptorSets[m_ImageIndex];
        writeDescriptorSet.dstBinding = 0;
        writeDescriptorSet.dstArrayElement = 0;
        writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writeDescriptorSet.descriptorCount = 1;
        writeDescriptorSet.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(
            m_Device->GetLogicalDeviceHandle(),
            1,
            &writeDescriptorSet,
            0,
            nullptr);

        const auto& cmdBuf = GetCurrentCommandBuffer();
        const auto& vkPipeline = std::dynamic_pointer_cast<VulkanPipeline>(pipeline);
        vkCmdBindDescriptorSets(
            cmdBuf.GetHandle(),
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            vkPipeline->GetPipelineLayout(),
            0,
            1,
            &m_GlobalDescriptorSets[m_ImageIndex],
            0,
            nullptr);
    }

    void VulkanRHI::DrawMesh(
        const Resources::GeometryRenderData& data,
        const Resources::PipelinePtr& pipeline)
    {
        data.Material->SetModel(*this, data.Model, pipeline);
        data.Mesh->Bind(*this);
        vkCmdDrawIndexed(
            GetCurrentCommandBuffer().GetHandle(),
            data.Mesh->GetIndicesCount(),
            1,
            0,
            0,
            0);
    }

    Resources::Texture2D* VulkanRHI::CreateTexture2D(
        const std::string& name,
        const uint32_t width,
        const uint32_t height,
        const uint8_t channels,
        const bool hasTransparency,
        const std::vector<uint8_t>& data)
    {
        return new VulkanTexture2D(
            *this,
            m_Device,
            m_Allocator,
            name,
            width,
            height,
            channels,
            hasTransparency,
            data);
    }

    Resources::IPipeline* VulkanRHI::CreatePipelineForRenderPass(
        PipelineConfig& config,
        Resources::IRenderPass* renderPass)
    {
        auto* vkRenderPass = dynamic_cast<VulkanRenderPass*>(renderPass);
        if (!vkRenderPass)
        {
            VA_ENGINE_ERROR("[VulkanRHI] Invalid render pass type.");
            return nullptr;
        }

        return new VulkanPipeline(config, m_Device, m_Allocator, vkRenderPass);
    }

    Resources::IMaterial* VulkanRHI::CreateMaterial(const std::string& name)
    {
        return new VulkanMaterial(name, m_Device, m_Allocator);
    }

    Resources::IShader* VulkanRHI::CreateShader(
        const std::string& name,
        const ShaderConfig& config,
        const std::vector<uint8_t>& data)
    {
        return new VulkanShader(m_Device, m_Allocator, name, config, data);
    }

    Resources::IMesh* VulkanRHI::CreateMesh(
        const std::string& name,
        const std::vector<Resources::MeshVertex>& vertices,
        const std::vector<uint32_t>& indices)
    {
        return new VulkanMesh(*this, m_Allocator, name, vertices, indices);
    }

    Resources::IRenderTarget* VulkanRHI::CreateRenderTarget(
        const Renderer::RenderTargetConfig& config)
    {
        if (config.IsMain)
        {
            // Main target: Use swapchain dimensions and format
            auto mainConfig = config;
            mainConfig.Width = m_FramebufferWidth;
            mainConfig.Height = m_FramebufferHeight;

            // Create the main render target (no attachment yet - managed by swapchain)
            const auto renderTarget = new VulkanRenderTarget(
                mainConfig,
                m_Device->GetLogicalDeviceHandle(),
                m_Allocator);

            m_MainRenderTargets.push_back(renderTarget);

            VA_ENGINE_TRACE(
                "[VulkanRHI] Main target '{}' created and added to registry.",
                config.Name);

            return renderTarget;
        }
        else if (!config.Attachments.empty())
        {
            // External textures provided
            std::vector<VkImageView> vulkanViews;
            vulkanViews.reserve(config.Attachments.size());

            for (const auto& texture : config.Attachments)
            {
                if (auto vulkanTexture = std::dynamic_pointer_cast<VulkanTexture2D>(texture))
                {
                    vulkanViews.push_back(vulkanTexture->GetImageView());
                }
                else
                {
                    VA_ENGINE_ERROR(
                        "[VulkanRHI] Invalid texture type for RenderTarget attachment.");
                    return nullptr;
                }
            }

            return new VulkanRenderTarget(
                config,
                m_Device->GetLogicalDeviceHandle(),
                m_Allocator,
                vulkanViews);
        }
        else
        {
            // Auto-create textures based on config
            std::vector<VkImageView> vulkanViews;

            VA_ENGINE_WARN(
                "[VulkanRHI] Auto texture creation not yet implented. Provide attachments explicitly for now.");
            return nullptr;
        }
    }

    Resources::IRenderPass* VulkanRHI::CreateRenderPass(const Renderer::RenderPassConfig& config)
    {
        const auto swapchainFormat = m_Swapchain->GetFormat();
        const auto depthFormat = m_Swapchain->GetDepthFormat();

        return new VulkanRenderPass(
            config,
            m_Device,
            m_Allocator,
            swapchainFormat,
            depthFormat);
    }

    int32_t VulkanRHI::FindMemoryIndex(
        const uint32_t typeFilter,
        const uint32_t propertyFlags) const
    {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(m_Device->GetPhysicalDeviceHandle(), &memProperties);

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
        VA_VULKAN_CHECK_RESULT_CRITICAL(vkEnumerateInstanceLayerProperties(&layerCount, nullptr));
        std::vector<VkLayerProperties> availableLayers(layerCount);
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
            {
                ss << extensions[i] << std::endl;
            }
            VA_ENGINE_CRITICAL(
                "[VulkanRHI] Failed to initialize instance.\nRequired extensions:\n%{}",
                ss.str());

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
        // TODO DeviceRequirements should be configurable by the Application, but for now we will
        //  keep it simple and make it directly here.
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
            VA_ENGINE_ERROR("[VulkanRHI] Failed to query surface formats.");
        }

        // --- Present modes ---
        uint32_t presentModeCount;
        VA_VULKAN_CHECK_RESULT_WARN(
            vkGetPhysicalDeviceSurfacePresentModesKHR(
                physicalDevice, surface, &presentModeCount, nullptr));

        if (presentModeCount != 0)
        {
            m_PresentModes.resize(presentModeCount);
            VA_VULKAN_CHECK_RESULT_WARN(
                vkGetPhysicalDeviceSurfacePresentModesKHR(
                    physicalDevice, surface, &presentModeCount, m_PresentModes.data()));
        }
        else
        {
            VA_ENGINE_ERROR("[VulkanRHI] Failed to query surface present modes.");
        }
    }

    VkSurfaceFormatKHR VulkanRHI::ChooseSwapchainFormat() const
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

    void VulkanRHI::CreateCommandBuffers()
    {
        m_GraphicsCommandBuffers.clear();

        const auto imageCount = m_Swapchain->GetImageCount();
        m_GraphicsCommandBuffers.reserve(imageCount);
        for (uint32_t i = 0; i < imageCount; i++)
        {
            m_GraphicsCommandBuffers.emplace_back(m_Device, m_Device->GetGraphicsCommandPool());
        }

        VA_ENGINE_INFO("[VulkanRHI] Command buffers created.");
    }

    void VulkanRHI::CreateSyncObjects()
    {
        const auto maxFrameInFlight = m_Swapchain->GetMaxFrameInFlight();
        m_ImageAvailableSemaphores.reserve(maxFrameInFlight);
        m_QueueCompleteSemaphores.reserve(maxFrameInFlight);
        m_InFlightFences.reserve(maxFrameInFlight);

        for (uint32_t i = 0; i < maxFrameInFlight; i++)
        {
            auto semaphoreCreateInfo = VkSemaphoreCreateInfo{};
            semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            VkSemaphore semaphore;
            VA_VULKAN_CHECK_RESULT_WARN(
                vkCreateSemaphore(
                    m_Device->GetLogicalDeviceHandle(), &semaphoreCreateInfo, m_Allocator, &
                    semaphore));
            m_ImageAvailableSemaphores.push_back(semaphore);

            VA_VULKAN_CHECK_RESULT_WARN(
                vkCreateSemaphore(
                    m_Device->GetLogicalDeviceHandle(), &semaphoreCreateInfo, m_Allocator, &
                    semaphore));
            m_QueueCompleteSemaphores.push_back(semaphore);

            m_InFlightFences.emplace_back(m_Device, m_Allocator, true);
        }

        m_ImagesInFlight.resize(m_Swapchain->GetImageCount());
        for (uint32_t i = 0; i < m_Swapchain->GetImageCount(); i++)
        {
            m_ImagesInFlight.emplace_back(nullptr);
        }

        VA_ENGINE_INFO("[VulkanRHI] Sync objects created.");
    }

    void VulkanRHI::CreateGlobalUBO()
    {
        // --- Global descriptor pool ---
        auto globalPoolSize = VkDescriptorPoolSize{};
        globalPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        globalPoolSize.descriptorCount = m_Swapchain->GetImageCount();

        auto globalPoolInfo = VkDescriptorPoolCreateInfo{};
        globalPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        globalPoolInfo.maxSets = m_Swapchain->GetImageCount();
        globalPoolInfo.poolSizeCount = 1;
        globalPoolInfo.pPoolSizes = &globalPoolSize;

        VA_VULKAN_CHECK_RESULT_WARN(
            vkCreateDescriptorPool(
                m_Device->GetLogicalDeviceHandle(),
                &globalPoolInfo,
                m_Allocator,
                &m_GlobalDescriptorPool));

        m_GlobalUniformBuffer = std::make_unique<VulkanBuffer>(
            *this,
            m_Device,
            m_Allocator,
            sizeof(Resources::GlobalUniformObject) * 3,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        const auto globalDescriptorSetLayout = g_VkDescriptorSetLayoutManager->GetGlobalLayout();
        const std::vector globalLayouts = {
            globalDescriptorSetLayout,
            globalDescriptorSetLayout,
            globalDescriptorSetLayout
        };

        m_GlobalDescriptorSets = new VkDescriptorSet[globalLayouts.size()];

        auto allocInfo = VkDescriptorSetAllocateInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_GlobalDescriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(globalLayouts.size());
        allocInfo.pSetLayouts = globalLayouts.data();
        VA_VULKAN_CHECK_RESULT_WARN(
            vkAllocateDescriptorSets(
                m_Device->GetLogicalDeviceHandle(), &allocInfo, m_GlobalDescriptorSets));
    }

    void VulkanRHI::DestroySyncObjects()
    {
        for (const auto& semaphore : m_ImageAvailableSemaphores)
        {
            vkDestroySemaphore(m_Device->GetLogicalDeviceHandle(), semaphore, m_Allocator);
        }
        for (const auto& semaphore : m_QueueCompleteSemaphores)
        {
            vkDestroySemaphore(m_Device->GetLogicalDeviceHandle(), semaphore, m_Allocator);
        }
        m_ImageAvailableSemaphores.clear();
        m_QueueCompleteSemaphores.clear();
        m_InFlightFences.clear();
        m_ImagesInFlight.clear();
    }

    void VulkanRHI::InvalidateMainTargetsFramebuffers() const
    {
        m_Device->WaitIdle();
        for (const auto& target : m_MainRenderTargets)
        {
            target->InvalidateFramebuffers();
        }

        VA_ENGINE_DEBUG("[VulkanRHI] Invalidated all main render target framebuffers.");
    }

    bool VulkanRHI::RecreateSwapchain()
    {
        if (m_RecreatingSwapchain)
        {
            VA_ENGINE_DEBUG("[VulkanRHI] RecreateSwapchain called when already recreating.");
            return false;
        }

        if (m_FramebufferWidth == 0 || m_FramebufferHeight == 0)
        {
            VA_ENGINE_DEBUG(
                "[VulkanRHI] RecreateSwapchain called when window is < 1 in a dimension.");
            return false;
        }

        m_RecreatingSwapchain = true;
        m_Device->WaitIdle();

        // Clear the ImageInFlight
        m_ImagesInFlight.clear();
        m_ImagesInFlight.resize(m_Swapchain->GetImageCount());

        // Requery support
        QuerySwapchainCapabilities();
        const auto depthFormat = ChooseDepthFormat();

        m_Swapchain->Recreate(
            *this,
            {m_CachedFramebufferWidth, m_CachedFramebufferHeight},
            depthFormat);

        // Sync
        m_FramebufferWidth = m_CachedFramebufferWidth;
        m_FramebufferHeight = m_CachedFramebufferHeight;
        m_CachedFramebufferWidth = 0;
        m_CachedFramebufferHeight = 0;

        // Update framebuffer size generation.
        m_FramebufferSizeLastGeneration = m_FramebufferSizeGeneration;

        m_GraphicsCommandBuffers.clear();

        CreateCommandBuffers();

        m_RecreatingSwapchain = false;
        VA_ENGINE_TRACE("[VulkanRHI] RecreateSwapchain finished.");

        return true;
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
            debugExtensions[extensionCount]);
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

    void VulkanRHI::DestroyDebugMessenger() const
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
} // namespace VoidArchitect::Platform
