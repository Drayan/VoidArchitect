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
        VulkanCommandBuffer(
            const std::unique_ptr<VulkanDevice>& device,
            VkCommandPool pool,
            bool isPrimary);
        ~VulkanCommandBuffer();

        void Begin(bool isSingleUse, bool isRenderPassContinue, bool isSimultaneousUse);
        void End();

        static void SingleUseBegin(
            const std::unique_ptr<VulkanDevice>& device,
            VkCommandPool pool,
            VulkanCommandBuffer& cmdBuf);
        static void SingleUseEnd(
            VulkanCommandBuffer& cmdBuf,
            VkQueue queue);

        void SetState(const CommandBufferState& state) { m_State = state; }

        CommandBufferState GetState() const { return m_State; }
        VkCommandBuffer GetHandle() const { return m_CommandBuffer; }

    private:
        VkDevice m_Device;
        VkCommandPool m_Pool;

        VkCommandBuffer m_CommandBuffer;
        CommandBufferState m_State = CommandBufferState::NotAllocated;
    };
} // VoidArchitect
