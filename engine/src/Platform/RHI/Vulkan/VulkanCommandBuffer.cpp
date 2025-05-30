//
// Created by Michael Desmedt on 19/05/2025.
//
#include "VulkanCommandBuffer.hpp"

#include "VulkanDevice.hpp"

#include <vulkan/vulkan.h>

#include "Core/Logger.hpp"
#include "VulkanUtils.hpp"

namespace VoidArchitect::Platform
{
    VulkanCommandBuffer::VulkanCommandBuffer(
        const std::unique_ptr<VulkanDevice>& device, const VkCommandPool pool, const bool isPrimary)
        : m_Device(device->GetLogicalDeviceHandle()),
          m_Pool(pool)
    {
        auto allocateInfo = VkCommandBufferAllocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocateInfo.commandPool = pool;
        allocateInfo.level =
            isPrimary ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
        allocateInfo.commandBufferCount = 1;

        VA_VULKAN_CHECK_RESULT_CRITICAL(
            vkAllocateCommandBuffers(m_Device, &allocateInfo, &m_CommandBuffer));
        m_State = CommandBufferState::Ready;

        VA_ENGINE_TRACE("[VulkanCommandBuffer] CommandBuffer created.");
    }

    VulkanCommandBuffer::VulkanCommandBuffer(VkDevice device, VkCommandPool pool, bool isPrimary)
        : m_Device(device),
          m_Pool(pool)
    {
        auto allocateInfo = VkCommandBufferAllocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocateInfo.commandPool = pool;
        allocateInfo.level =
            isPrimary ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
        allocateInfo.commandBufferCount = 1;

        VA_VULKAN_CHECK_RESULT_CRITICAL(
            vkAllocateCommandBuffers(m_Device, &allocateInfo, &m_CommandBuffer));
        m_State = CommandBufferState::Ready;

        VA_ENGINE_TRACE("[VulkanCommandBuffer] CommandBuffer created.");
    }

    VulkanCommandBuffer::VulkanCommandBuffer(VulkanCommandBuffer&& other) noexcept
        : m_Device{other.m_Device},
          m_Pool{other.m_Pool},
          m_CommandBuffer{other.m_CommandBuffer},
          m_State{other.m_State}
    {
        other.InvalidateResources();
        VA_ENGINE_TRACE("[VulkanCommandBuffer] CommandBuffer moved.");
    }

    VulkanCommandBuffer& VulkanCommandBuffer::operator=(VulkanCommandBuffer&& other) noexcept
    {
        if (this != &other)
        {
            this->~VulkanCommandBuffer();

            m_Device = other.m_Device;
            m_Pool = other.m_Pool;
            m_CommandBuffer = other.m_CommandBuffer;
            m_State = other.m_State;

            other.InvalidateResources();
            VA_ENGINE_TRACE("[VulkanCommandBuffer] CommandBuffer moved.");
        }

        return *this;
    }

    VulkanCommandBuffer::~VulkanCommandBuffer()
    {
        if (m_CommandBuffer != VK_NULL_HANDLE)
        {
            vkFreeCommandBuffers(m_Device, m_Pool, 1, &m_CommandBuffer);
            m_CommandBuffer = VK_NULL_HANDLE;
            m_State = CommandBufferState::NotAllocated;

            VA_ENGINE_TRACE("[VulkanCommandBuffer] CommandBuffer destroyed.");
        }
    }

    void VulkanCommandBuffer::InvalidateResources()
    {
        m_Device = VK_NULL_HANDLE;
        m_Pool = VK_NULL_HANDLE;
        m_CommandBuffer = VK_NULL_HANDLE;
    }

    void VulkanCommandBuffer::Begin(
        const bool isSingleUse, const bool isRenderPassContinue, const bool isSimultaneousUse)
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

        VA_VULKAN_CHECK_RESULT_WARN(vkBeginCommandBuffer(m_CommandBuffer, &beginInfo));
        m_State = CommandBufferState::Recording;
    }

    void VulkanCommandBuffer::End()
    {
        VA_VULKAN_CHECK_RESULT_WARN(vkEndCommandBuffer(m_CommandBuffer));
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

    void VulkanCommandBuffer::SingleUseBegin(
        const VkDevice device, const VkCommandPool pool, VulkanCommandBuffer& cmdBuf)
    {
        cmdBuf = VulkanCommandBuffer(device, pool);
        cmdBuf.Begin(true);
    }

    void VulkanCommandBuffer::SingleUseEnd(
        VulkanCommandBuffer& cmdBuf, const VkQueue queue, const VkFence fence)
    {
        cmdBuf.End();

        const auto handle = cmdBuf.GetHandle();
        auto submitInfo = VkSubmitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &handle;
        VA_VULKAN_CHECK_RESULT_WARN(vkQueueSubmit(queue, 1, &submitInfo, fence));

        // Wait for it to finish
        VA_VULKAN_CHECK_RESULT_WARN(vkQueueWaitIdle(queue));
    }
} // namespace VoidArchitect::Platform
