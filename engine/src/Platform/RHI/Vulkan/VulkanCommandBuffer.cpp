//
// Created by Michael Desmedt on 19/05/2025.
//
#include "VulkanCommandBuffer.hpp"

#include "VulkanDevice.hpp"

#include <vulkan/vulkan.h>

#include "Core/Logger.hpp"

namespace VoidArchitect::Platform
{
    VulkanCommandBuffer::VulkanCommandBuffer(
        const std::unique_ptr<VulkanDevice>& device,
        const VkCommandPool pool,
        const bool isPrimary)
        : m_Device(device->GetLogicalDeviceHandle()),
          m_Pool(pool)
    {
        auto allocateInfo = VkCommandBufferAllocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocateInfo.commandPool = pool;
        allocateInfo.level = isPrimary
                                 ? VK_COMMAND_BUFFER_LEVEL_PRIMARY
                                 : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
        allocateInfo.commandBufferCount = 1;

        vkAllocateCommandBuffers(m_Device, &allocateInfo, &m_CommandBuffer);
        m_State = CommandBufferState::Ready;
    }

    VulkanCommandBuffer::~VulkanCommandBuffer()
    {
        vkFreeCommandBuffers(m_Device, m_Pool, 1, &m_CommandBuffer);
        m_CommandBuffer = VK_NULL_HANDLE;
        m_State = CommandBufferState::NotAllocated;
    }

    void VulkanCommandBuffer::Begin(
        const bool isSingleUse = false,
        const bool isRenderPassContinue = false,
        const bool isSimultaneousUse = false)
    {
        auto beginInfo = VkCommandBufferBeginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;
        if (isSingleUse)
        {
            beginInfo.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        }
        if (isRenderPassContinue)
        {
            beginInfo.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
        }
        if (isSimultaneousUse)
        {
            beginInfo.flags |= VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
        }

        if (vkBeginCommandBuffer(m_CommandBuffer, &beginInfo) != VK_SUCCESS)
        {
            VA_ENGINE_WARN("[VulkanCommandBuffer] Failed to begin command buffer.");
            return;
        }
        m_State = CommandBufferState::Recording;
    }

    void VulkanCommandBuffer::End()
    {
        if (vkEndCommandBuffer(m_CommandBuffer) != VK_SUCCESS)
        {
            VA_ENGINE_WARN("[VulkanCommandBuffer] Failed to end command buffer.");
            return;
        }
        m_State = CommandBufferState::RecordingEnded;
    }

    void VulkanCommandBuffer::SingleUseBegin(
        const std::unique_ptr<VulkanDevice>& device,
        const VkCommandPool pool,
        VulkanCommandBuffer& cmdBuf)
    {
        cmdBuf = VulkanCommandBuffer(device, pool);
        cmdBuf.Begin(true);
    }

    void VulkanCommandBuffer::SingleUseEnd(
        VulkanCommandBuffer& cmdBuf,
        const VkQueue queue)
    {
        cmdBuf.End();

        const auto handle = cmdBuf.GetHandle();
        auto submitInfo = VkSubmitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &handle;
        if (vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS)
        {
            VA_ENGINE_WARN("[VulkanCommandBuffer] Failed to submit command buffer.");
            return;
        }

        // Wait for it to finish
        if (vkQueueWaitIdle(queue) != VK_SUCCESS)
        {
            VA_ENGINE_WARN("[VulkanCommandBuffer] Failed to wait for command buffer.");
            return;
        }
    }
} // VoidArchitect
