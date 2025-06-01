//
// Created by Michael Desmedt on 14/05/2025.
//
#pragma once
#include <vulkan/vulkan.h>

#include "VulkanCommandBuffer.hpp"
#include "VulkanDevice.hpp"
#include "Platform/RHI/IRenderingHardware.hpp"

namespace VoidArchitect
{
    struct PipelineInputLayout;
    class Window;
    struct GeometryRenderData;

    namespace Resources
    {
        class Texture2D;
        class IMaterial;
        class IPipeline;
    }
}

namespace VoidArchitect::Platform
{
    class VulkanSwapchain;
    class VulkanRenderpass;
    class VulkanPipeline;
    class VulkanFence;
    class VulkanShader;
    class VulkanVertexBuffer;
    class VulkanIndexBuffer;
    class VulkanBuffer;
    class VulkanMaterial;

    class VulkanRHI final : public IRenderingHardware
    {
    public:
        explicit VulkanRHI(
            std::unique_ptr<Window>& window,
            const PipelineInputLayout& sharedInputLayout);
        ~VulkanRHI() override;

        void Resize(uint32_t width, uint32_t height) override;
        void WaitIdle(uint64_t timeout) override;

        bool BeginFrame(float deltaTime) override;
        bool EndFrame(float deltaTime) override;

        void UpdateGlobalState(
            const Resources::PipelinePtr& pipeline,
            const Math::Mat4& projection,
            const Math::Mat4& view) override;

        void DrawMesh(const Resources::GeometryRenderData& data) override;

        ///////////////////////////////////////////////////////////////////////
        //// Resources ////////////////////////////////////////////////////////
        ///////////////////////////////////////////////////////////////////////
        Resources::Texture2D* CreateTexture2D(
            const std::string& name,
            uint32_t width,
            uint32_t height,
            uint8_t channels,
            bool hasTransparency,
            const std::vector<uint8_t>& data) override;
        Resources::IPipeline* CreatePipeline(PipelineConfig& config) override;
        Resources::IMaterial* CreateMaterial(
            const std::string& name,
            const Resources::PipelinePtr& pipeline) override;
        Resources::IShader* CreateShader(
            const std::string& name,
            const ShaderConfig& config,
            const std::vector<uint8_t>& data) override;
        Resources::IMesh* CreateMesh(
            const std::string& name,
            const std::vector<Resources::MeshVertex>& vertices,
            const std::vector<uint32_t>& indices) override;

        [[nodiscard]] VkSurfaceCapabilitiesKHR GetSwapchainCapabilities() const
        {
            return m_Capabilities;
        }

        void SetImageIndex(const uint32_t index) { m_ImageIndex = index; }
        void SetCurrentIndex(const uint32_t index) { m_CurrentIndex = index; }

        VulkanCommandBuffer& GetCurrentCommandBuffer()
        {
            return m_GraphicsCommandBuffers[m_ImageIndex];
        }

        std::unique_ptr<VulkanDevice>& GetDeviceRef() { return m_Device; }
        std::unique_ptr<VulkanSwapchain>& GetSwapchainRef() { return m_Swapchain; }
        std::unique_ptr<VulkanRenderpass>& GetMainRenderpassRef() { return m_MainRenderpass; }
        [[nodiscard]] uint32_t GetImageIndex() const { return m_ImageIndex; }

        [[nodiscard]] int32_t FindMemoryIndex(uint32_t typeFilter, uint32_t propertyFlags) const;

    private:
        void CreateInstance();
        void CreateDevice(std::unique_ptr<Window>& window);
        void CreateSwapchain();

        void QuerySwapchainCapabilities();

        [[nodiscard]] VkSurfaceFormatKHR ChooseSwapchainFormat() const;
        [[nodiscard]] VkPresentModeKHR ChooseSwapchainPresentMode() const;
        [[nodiscard]] VkExtent2D ChooseSwapchainExtent() const;
        [[nodiscard]] VkFormat ChooseDepthFormat() const;

        void CreateRenderpass();
        void CreateCommandBuffers();
        void CreateSyncObjects();

        // TEMP This should not stay here.
        void CreateGlobalUBO();

        void DestroySyncObjects();

        bool RecreateSwapchain();

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

        // TEMP Temporary test code
        VkDescriptorPool m_GlobalDescriptorPool;
        VkDescriptorSet* m_GlobalDescriptorSets;

        std::unique_ptr<VulkanBuffer> m_GlobalUniformBuffer;

        uint32_t m_FramebufferWidth;
        uint32_t m_FramebufferHeight;
        uint32_t m_CachedFramebufferWidth;
        uint32_t m_CachedFramebufferHeight;
        uint64_t m_FramebufferSizeGeneration;
        uint64_t m_FramebufferSizeLastGeneration;
    };
} // namespace VoidArchitect::Platform
