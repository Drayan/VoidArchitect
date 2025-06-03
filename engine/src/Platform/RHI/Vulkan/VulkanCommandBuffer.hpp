//
// Created by Michael Desmedt on 19/05/2025.
//
#pragma once

#include <vulkan/vulkan.h>

namespace VoidArchitect::Platform
{
    class VulkanDevice;

    enum class CommandBufferState
    {
        Ready,
        Recording,
        InRenderpass,
        RecordingEnded,
        Submitted,
        NotAllocated
    };

    class VulkanCommandBuffer
    {
    public:
        VulkanCommandBuffer() = default;
        VulkanCommandBuffer(
            const std::unique_ptr<VulkanDevice>& device, VkCommandPool pool, bool isPrimary = true);
        VulkanCommandBuffer(VkDevice device, VkCommandPool pool, bool isPrimary = true);
        VulkanCommandBuffer(VulkanCommandBuffer&& other) noexcept;
        VulkanCommandBuffer(const VulkanCommandBuffer& other) = delete;
        ~VulkanCommandBuffer();

        VulkanCommandBuffer& operator=(VulkanCommandBuffer&& other) noexcept;
        VulkanCommandBuffer& operator=(const VulkanCommandBuffer& other) = delete;

        void Begin(
            bool isSingleUse = false,
            bool isRenderPassContinue = false,
            bool isSimultaneousUse = false);
        void End();
        void Reset() { m_State = CommandBufferState::Ready; };

        static void SingleUseBegin(
            const std::unique_ptr<VulkanDevice>& device,
            VkCommandPool pool,
            VulkanCommandBuffer& cmdBuf);
        static void SingleUseBegin(
            VkDevice device, VkCommandPool pool, VulkanCommandBuffer& cmdBuf);
        static void SingleUseEnd(VulkanCommandBuffer& cmdBuf, VkQueue queue, VkFence fence);

        void SetState(const CommandBufferState& state) { m_State = state; }

        CommandBufferState GetState() const { return m_State; }
        VkCommandBuffer GetHandle() const { return m_CommandBuffer; }

    private:
        void InvalidateResources();

        VkDevice m_Device;
        VkCommandPool m_Pool;

        VkCommandBuffer m_CommandBuffer{};
        CommandBufferState m_State = CommandBufferState::NotAllocated;
    };
} // namespace VoidArchitect::Platform
