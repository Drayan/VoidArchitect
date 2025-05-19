//
// Created by Michael Desmedt on 19/05/2025.
//
#pragma once
#include <vulkan/vulkan.h>

#include "VulkanCommandBuffer.hpp"

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

    class VulkanRenderpass
    {
    public:
        VulkanRenderpass(
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
        ~VulkanRenderpass();

        void Begin(VulkanCommandBuffer& cmdBuf, VkFramebuffer framebuffer) const;
        void End(VulkanCommandBuffer& cmdBuf) const;

        VkRenderPass GetHandle() const { return m_Renderpass; }

        void SetWidth(const uint32_t width) { m_w = width; };
        void SetHeight(const uint32_t height) { m_h = height; };
        void SetX(const uint32_t x) { m_x = x; };
        void SetY(const uint32_t y) { m_y = y; };

    private:
        VkDevice m_Device;
        VkAllocationCallbacks* m_Allocator;

        RenderpassState m_State = RenderpassState::NotAllocated;
        VkRenderPass m_Renderpass;

        int32_t m_x, m_y;
        uint32_t m_w, m_h;
        std::vector<VkClearValue> m_ClearValues;
    };
} // VoidArchitect
