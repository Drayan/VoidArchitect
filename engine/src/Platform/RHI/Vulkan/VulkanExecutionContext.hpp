//
// Created by Michael Desmedt on 08/06/2025.
//
#pragma once

#include <vulkan/vulkan.h>

#include "VulkanBindingGroupManager.hpp"
#include "VulkanCommandBuffer.hpp"
#include "Resources/Material.hpp"
#include "Resources/RenderTarget.hpp"
#include "Systems/RenderPassSystem.hpp"
#include "Systems/RenderStateSystem.hpp"

namespace VoidArchitect::Platform
{
    class VulkanResourceFactory;
}

namespace VoidArchitect::Platform
{
    class VulkanFramebufferCache;
}

namespace VoidArchitect::Resources
{
    struct GlobalUniformObject;
}

namespace VoidArchitect::Platform
{
    class VulkanSwapchain;
    class VulkanDevice;
    class VulkanFence;
    class VulkanBuffer;

    class VulkanExecutionContext
    {
    public:
        VulkanExecutionContext(
            const std::unique_ptr<VulkanDevice>& device,
            VkAllocationCallbacks* allocator,
            uint32_t width,
            uint32_t height);
        ~VulkanExecutionContext();

        bool BeginFrame(float deltaTime);
        bool EndFrame(float deltaTime);

        void BeginRenderPass(
            RenderPassHandle passHandle,
            const VAArray<Resources::RenderTargetHandle>& targetHandles);
        void EndRenderPass() const;

        void UpdateGlobalState(const Resources::GlobalUniformObject& gUBO) const;

        void BindRenderState(RenderStateHandle stateHandle);
        void BindMaterialGroup(MaterialHandle materialHandle, RenderStateHandle stateHandle);
        bool BindMesh(Resources::MeshHandle meshHandle);
        void PushConstants(Resources::ShaderStage stage, uint32_t size, const void* data);
        void DrawIndexed(
            uint32_t indexCount,
            uint32_t indexOffset = 0,
            uint32_t vertexOffset = 0,
            uint32_t instanceCount = 1,
            uint32_t firstInstance = 0);

        void RequestResize(const uint32_t width, const uint32_t height);

        VulkanCommandBuffer& GetCurrentCommandBuffer()
        {
            return m_GraphicsCommandBuffers[m_ImageIndex];
        }

        [[nodiscard]] uint32_t GetImageIndex() const { return m_ImageIndex; }
        Resources::RenderTargetHandle GetCurrentColorRenderTargetHandle() const;
        Resources::RenderTargetHandle GetDepthRenderTargetHandle() const;
        VkFormat GetSwapchainFormat() const;
        VkFormat GetDepthFormat() const;
        VkDescriptorSetLayout GetGlobalSetLayout() const { return m_GlobalDescriptorSetLayout; }

    private:
        void CreateSyncObjects();
        void CreateCommandBuffers();
        void CreateGlobalUBO();

        void HandleResize();

        void DestroySyncObjects();

        struct AttachmentDescriptor
        {
            std::string name;
            VkFormat resolvedFormat;
            bool isDepthAttachment;
            uint32_t configIndex;
        };

        VAArray<VkImageView> SortAttachmentsToMatchRenderPassOrder(
            const Renderer::RenderPassConfig& config,
            const VAArray<Resources::RenderTargetHandle>& targetHandles) const;
        VkFormat ResolveAttachmentFormat(Renderer::TextureFormat engineFormat) const;
        void ValidateAttachmentCompatibility(
            const AttachmentDescriptor& expected,
            const Resources::IRenderTarget* renderTarget) const;

        const std::unique_ptr<VulkanDevice>& m_Device;
        VkAllocationCallbacks* m_Allocator;

        uint32_t m_ImageIndex = 0;
        uint32_t m_CurrentIndex = 0;
        bool m_RecreatingSwapchain = false;

        uint32_t m_CurrentWidth = 0;
        uint32_t m_CurrentHeight = 0;
        uint32_t m_PendingWidth = 0;
        uint32_t m_PendingHeight = 0;
        uint64_t m_ProcessedSizeGeneration = 0;
        uint64_t m_RequestedSizeGeneration = 0;
        std::unique_ptr<VulkanSwapchain> m_Swapchain;

        VAArray<VkSemaphore> m_ImageAvailableSemaphores;
        VAArray<VkSemaphore> m_QueueCompleteSemaphores;

        VAArray<VulkanFence> m_InFlightFences;
        VAArray<VulkanFence*> m_ImagesInFlight;
        VAArray<VulkanFence> m_ImageAcquisitionFences;

        VAArray<VulkanCommandBuffer> m_GraphicsCommandBuffers;
        VkPipelineLayout m_LastBoundPipelineLayout = VK_NULL_HANDLE;

        VkDescriptorPool m_GlobalDescriptorPool;
        VkDescriptorSetLayout m_GlobalDescriptorSetLayout;
        VkDescriptorSet* m_GlobalDescriptorSets;
        std::unique_ptr<VulkanBuffer> m_GlobalUniformBuffer;

        std::unique_ptr<VulkanFramebufferCache> m_FramebufferCache;
    };

    inline std::unique_ptr<VulkanExecutionContext> g_VkExecutionContext;
}
