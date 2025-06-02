//
// Created by Michael Desmedt on 19/05/2025.
//
#pragma once
#include <vulkan/vulkan.h>

#include "VulkanCommandBuffer.hpp"
#include "Resources/RenderPass.hpp"
#include "Systems/Renderer/RenderGraph.hpp"

namespace VoidArchitect::Platform
{
    class VulkanSwapchain;
    class VulkanDevice;

    enum class RenderpassState
    {
        Ready,
        Recording,
        InRenderpass,
        RecordingEnded,
        Submitted,
        NotAllocated
    };

    class VulkanRenderPass : public Resources::IRenderPass
    {
    public:
        // New constructor using RenderPassConfig (preferred)
        VulkanRenderPass(
            const Renderer::RenderPassConfig& config,
            const std::unique_ptr<VulkanDevice>& device,
            VkAllocationCallbacks* allocator,
            VkFormat swapchainFormat,
            VkFormat depthFormat);

        // Legacy constructor for backward compatibility (deprecated)
        VulkanRenderPass(
            const std::unique_ptr<VulkanDevice>& device,
            const std::unique_ptr<VulkanSwapchain>& swapchain,
            VkAllocationCallbacks* allocator,
            int32_t x,
            int32_t y,
            uint32_t w,
            uint32_t h,
            float r,
            float g,
            float b,
            float a,
            float depth,
            uint32_t stencil);
        ~VulkanRenderPass() override;

        void Begin(IRenderingHardware& rhi, const Resources::RenderTargetPtr& target) override;
        void End(IRenderingHardware& rhi) override;

        [[nodiscard]] bool IsCompatibleWith(
            const Resources::RenderTargetPtr& target) const override;

        // Legacy vulkan-specific methods (for backward compatibility)
        void Begin(VulkanCommandBuffer& cmdBuf, VkFramebuffer framebuffer) const;
        void End(VulkanCommandBuffer& cmdBuf) const;

        // Vulkan-specific getters
        VkRenderPass GetHandle() const { return m_Renderpass; }

        // Configuration access
        void SetDimensions(const uint32_t x, const uint32_t y, const uint32_t w, const uint32_t h)
        {
            m_x = x;
            m_y = y;
            m_w = w;
            m_h = h;
        }

        void SetWidth(const uint32_t width) { m_w = width; };
        void SetHeight(const uint32_t height) { m_h = height; };
        void SetX(const uint32_t x) { m_x = x; };
        void SetY(const uint32_t y) { m_y = y; };

    protected:
        void Release() override;

    private:
        void CreateRenderPassFromConfig(
            const Renderer::RenderPassConfig& config,
            VkFormat swapchainFormat,
            VkFormat depthFormat);
        void CreateLegacyRenderPass(
            const std::unique_ptr<VulkanSwapchain>& swapchain,
            float r,
            float g,
            float b,
            float a,
            float depth,
            uint32_t stencil);

        VkDevice m_Device;
        VkAllocationCallbacks* m_Allocator;

        RenderpassState m_State = RenderpassState::NotAllocated;
        VkRenderPass m_Renderpass = VK_NULL_HANDLE;

        int32_t m_x, m_y;
        uint32_t m_w, m_h;
        std::vector<VkClearValue> m_ClearValues;

        // Store config for compatibility checks
        Renderer::RenderPassConfig m_Config;
        bool m_IsLegacy = false;

        Resources::RenderTargetPtr m_CurrentTarget;
    };
} // namespace VoidArchitect::Platform
