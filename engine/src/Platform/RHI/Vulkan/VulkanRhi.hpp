//
// Created by Michael Desmedt on 14/05/2025.
//
#pragma once
#include <vulkan/vulkan.h>

#include "Platform/RHI/IRenderingHardware.hpp"
#include "VulkanCommandBuffer.hpp"
#include "VulkanDevice.hpp"
#include "VulkanRenderTarget.hpp"

namespace VoidArchitect
{
    struct RenderStateInputLayout;
    class Window;
    struct GeometryRenderData;

    namespace Resources
    {
        class Texture2D;
        class IMaterial;
        class IRenderState;
        struct GlobalUniformObject;
    } // namespace Resources
} // namespace VoidArchitect

namespace VoidArchitect::Platform
{
    class VulkanSwapchain;
    class VulkanRenderPass;
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
            const RenderStateInputLayout& sharedInputLayout);
        ~VulkanRHI() override;

        void Resize(uint32_t width, uint32_t height) override;
        void WaitIdle(uint64_t timeout) override;

        bool BeginFrame(float deltaTime) override;
        bool EndFrame(float deltaTime) override;

        void UpdateGlobalState(const Resources::GlobalUniformObject& gUBO) override;
        void BindGlobalState(const Resources::RenderStatePtr& pipeline) override;

        void DrawMesh(
            const Resources::GeometryRenderData& data,
            const Resources::RenderStatePtr& pipeline) override;

        ///////////////////////////////////////////////////////////////////////
        //// Resources ////////////////////////////////////////////////////////
        ///////////////////////////////////////////////////////////////////////
        Resources::Texture2D* CreateTexture2D(
            const std::string& name,
            uint32_t width,
            uint32_t height,
            uint8_t channels,
            bool hasTransparency,
            const VAArray<uint8_t>& data) override;
        Resources::IRenderState* CreatePipeline(
            RenderStateConfig& config,
            Resources::IRenderPass* renderPass) override;
        Resources::IMaterial* CreateMaterial(const std::string& name) override;
        Resources::IShader* CreateShader(
            const std::string& name,
            const ShaderConfig& config,
            const VAArray<uint8_t>& data) override;
        Resources::IMesh* CreateMesh(
            const std::string& name,
            const VAArray<Resources::MeshVertex>& vertices,
            const VAArray<uint32_t>& indices) override;

        ///////////////////////////////////////////////////////////////////////
        //// RenderGraph Resources ////////////////////////////////////////////
        ///////////////////////////////////////////////////////////////////////
        Resources::IRenderTarget* CreateRenderTarget(
            const Renderer::RenderTargetConfig& config) override;
        Resources::IRenderPass* CreateRenderPass(
            const RenderPassConfig& config,
            const Renderer::PassPosition passPosition) override;

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

        void CreateCommandBuffers();
        void CreateSyncObjects();

        // TEMP This should not stay here.
        void CreateGlobalUBO();

        void DestroySyncObjects();

        void InvalidateMainTargetsFramebuffers() const;
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
        VAArray<VkSurfaceFormatKHR> m_Formats;
        VAArray<VkPresentModeKHR> m_PresentModes;

        uint32_t m_ImageIndex;
        uint32_t m_CurrentIndex;
        bool m_RecreatingSwapchain = false;
        std::unique_ptr<VulkanSwapchain> m_Swapchain;
        VAArray<VulkanRenderTarget*> m_MainRenderTargets;
        VAArray<VulkanCommandBuffer> m_GraphicsCommandBuffers;

        VAArray<VkSemaphore> m_ImageAvailableSemaphores;
        VAArray<VkSemaphore> m_QueueCompleteSemaphores;

        VAArray<VulkanFence> m_InFlightFences;
        VAArray<VulkanFence*> m_ImagesInFlight;

        // TEMP Temporary test code
        VkDescriptorPool m_GlobalDescriptorPool;
        VkDescriptorSet* m_GlobalDescriptorSets;

        std::unique_ptr<VulkanBuffer> m_GlobalUniformBuffer;

        uint32_t m_CurrentWidth;
        uint32_t m_CurrentHeight;
        uint32_t m_PendingWidth;
        uint32_t m_PendingHeight;
        uint64_t m_ResizeGeneration;
        uint64_t m_ResizeLastGeneration;
    };
} // namespace VoidArchitect::Platform
