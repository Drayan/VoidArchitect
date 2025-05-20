//
// Created by Michael Desmedt on 14/05/2025.
//
#pragma once
#include "Platform/RHI/IRenderingHardware.hpp"

#include "VulkanDevice.hpp"

#include <vulkan/vulkan.h>

#include "VulkanBuffer.hpp"
#include "Core/Math/Mat4.hpp"

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
    class VulkanFence;
    class VulkanShader;
    class VulkanVertexBuffer;
    class VulkanIndexBuffer;
    class VulkanBuffer;

    //TEMP This should not stay here.
    //NOTE Vulkan give us a max of 256 bytes on G_UBO
    struct GlobalUniformObject
    {
        Math::Mat4 Projection;
        Math::Mat4 View;
        Math::Mat4 Reserved0;
        Math::Mat4 Reserved1;
    };

    class VulkanRHI final : public IRenderingHardware
    {
    public:
        explicit VulkanRHI(std::unique_ptr<Window>& window);
        ~VulkanRHI() override;

        void Resize(uint32_t width, uint32_t height) override;

        bool BeginFrame(float deltaTime) override;
        bool EndFrame(float deltaTime) override;

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
        void CreateSyncObjects();

        void CreatePipeline();

        void DestroySyncObjects();

        bool RecreateSwapchain();

        //TEMP This method should be moved elsewhere
        void UpdateGlobalState(const GlobalUniformObject& globalUniformObject) const;

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
        bool m_RecreatingSwapchain = false;
        std::unique_ptr<VulkanSwapchain> m_Swapchain;
        std::unique_ptr<VulkanRenderpass> m_MainRenderpass;
        std::vector<VulkanCommandBuffer> m_GraphicsCommandBuffers;

        std::vector<VkSemaphore> m_ImageAvailableSemaphores;
        std::vector<VkSemaphore> m_QueueCompleteSemaphores;

        std::vector<VulkanFence> m_InFlightFences;
        std::vector<VulkanFence*> m_ImagesInFlight;

        std::vector<VulkanShader> m_Shaders;
        std::unique_ptr<VulkanPipeline> m_Pipeline;

        //TEMP These should not stay here
        VkDescriptorPool m_DescriptorPool;
        VkDescriptorSet* m_DescriptorSets;
        VkDescriptorSetLayout m_DescriptorSetLayout;
        GlobalUniformObject m_GlobalUniformObject;
        std::unique_ptr<VulkanBuffer> m_GlobalUniformBuffer;

        std::unique_ptr<VulkanVertexBuffer> m_VertexBuffer;
        std::unique_ptr<VulkanIndexBuffer> m_IndexBuffer;

        uint32_t m_FramebufferWidth;
        uint32_t m_FramebufferHeight;
        uint32_t m_CachedFramebufferWidth;
        uint32_t m_CachedFramebufferHeight;
        uint64_t m_FramebufferSizeGeneration;
        uint64_t m_FramebufferSizeLastGeneration;
    };
} // VoidArchitect
