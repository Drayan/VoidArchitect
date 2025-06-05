//
// Created by Michael Desmedt on 19/05/2025.
//
#include "VulkanBuffer.hpp"

#include "Core/Logger.hpp"
#include "VulkanCommandBuffer.hpp"
#include "VulkanDevice.hpp"
#include "VulkanFence.hpp"
#include "VulkanRhi.hpp"
#include "VulkanUtils.hpp"

namespace VoidArchitect::Platform
{
    VulkanBuffer::VulkanBuffer(
        const VulkanRHI& rhi,
        const std::unique_ptr<VulkanDevice>& device,
        VkAllocationCallbacks* allocator,
        const uint64_t size,
        const VkBufferUsageFlags usage,
        VkMemoryPropertyFlags memProperties,
        const bool bindOnCreate)
        : m_Device(device->GetLogicalDeviceHandle()),
          m_Allocator(allocator),
          m_Buffer{},
          m_Memory{},
          m_Offset{0},
          m_Size{size},
          m_Usage{usage},
          m_MemoryProperties{memProperties}
    {
        auto bufferCreateInfo = VkBufferCreateInfo{};
        bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreateInfo.size = m_Size;
        bufferCreateInfo.usage = usage;
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VA_VULKAN_CHECK_RESULT_CRITICAL(
            vkCreateBuffer(m_Device, &bufferCreateInfo, m_Allocator, &m_Buffer));

        // Gather memory requirements.
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(m_Device, m_Buffer, &memRequirements);
        const auto memoryIndex = rhi.FindMemoryIndex(memRequirements.memoryTypeBits, memProperties);
        if (memoryIndex == -1)
        {
            VA_ENGINE_CRITICAL("[VulkanBuffer] Failed to find memory type index.");
            return;
        }

        // Allocate memory
        auto allocateInfo = VkMemoryAllocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocateInfo.allocationSize = memRequirements.size;
        allocateInfo.memoryTypeIndex = memoryIndex;
        VA_VULKAN_CHECK_RESULT_CRITICAL(
            vkAllocateMemory(m_Device, &allocateInfo, m_Allocator, &m_Memory));

        if (bindOnCreate)
        {
            BindMemory();
        }
    }

    VulkanBuffer::~VulkanBuffer()
    {
        if (m_Memory != VK_NULL_HANDLE)
        {
            vkFreeMemory(m_Device, m_Memory, m_Allocator);
            VA_ENGINE_TRACE("[VulkanBuffer] Memory released.");
        }

        if (m_Buffer != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(m_Device, m_Buffer, m_Allocator);
            VA_ENGINE_TRACE("[VulkanBuffer] Buffer destroyed.");
        }
    }

    VulkanBuffer::VulkanBuffer(VulkanBuffer&& other) noexcept
        : m_Device(other.m_Device),
          m_Allocator(other.m_Allocator),
          m_Buffer(other.m_Buffer),
          m_Memory{other.m_Memory},
          m_Offset{other.m_Offset},
          m_Size{other.m_Size},
          m_Usage{other.m_Usage},
          m_MemoryProperties{other.m_MemoryProperties}
    {
        other.InvalidateResources();
        VA_ENGINE_TRACE("[VulkanBuffer] Buffer moved.");
    }

    VulkanBuffer& VulkanBuffer::operator=(VulkanBuffer&& other) noexcept
    {
        if (this != &other)
        {
            this->~VulkanBuffer();

            m_Device = other.m_Device;
            m_Allocator = other.m_Allocator;
            m_Buffer = other.m_Buffer;
            m_Memory = other.m_Memory;
            m_Offset = other.m_Offset;
            m_Size = other.m_Size;
            m_Usage = other.m_Usage;
            m_MemoryProperties = other.m_MemoryProperties;

            other.InvalidateResources();
        }

        return *this;
    }

    void VulkanBuffer::InvalidateResources()
    {
        m_Buffer = VK_NULL_HANDLE;
        m_Memory = VK_NULL_HANDLE;
    }

    bool VulkanBuffer::Resize(
        const VulkanRHI& rhi,
        const uint64_t newSize,
        const VkQueue queue,
        const VkCommandPool pool)
    {
        // Create a new buffer.
        auto bufferCreateInfo = VkBufferCreateInfo{};
        bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreateInfo.size = newSize;
        bufferCreateInfo.usage = m_Usage;
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VkBuffer newBuffer;

        VA_VULKAN_CHECK_RESULT_CRITICAL(
            vkCreateBuffer(m_Device, &bufferCreateInfo, m_Allocator, &newBuffer));

        // Gather memory requirements.
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(m_Device, newBuffer, &memRequirements);
        const auto memoryIndex =
            rhi.FindMemoryIndex(memRequirements.memoryTypeBits, m_MemoryProperties);
        if (memoryIndex == -1)
        {
            VA_ENGINE_CRITICAL("[VulkanBuffer] Failed to find memory type index.");
            return false;
        }

        // Allocate memory
        VkDeviceMemory newMemory;
        auto allocateInfo = VkMemoryAllocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocateInfo.allocationSize = memRequirements.size;
        allocateInfo.memoryTypeIndex = memoryIndex;
        VA_VULKAN_CHECK_RESULT_CRITICAL(
            vkAllocateMemory(m_Device, &allocateInfo, m_Allocator, &newMemory));

        // Bind the memory
        VA_VULKAN_CHECK_RESULT_WARN(vkBindBufferMemory(m_Device, m_Buffer, newMemory, 0));

        // Create a temporary fence
        auto fence = VulkanFence(m_Device, m_Allocator);
        // Copy over the data
        this->CopyTo(pool, fence.GetHandle(), queue, newBuffer, 0, newSize);
        fence.Wait(10);

        // Destroy the old buffer
        vkDestroyBuffer(m_Device, m_Buffer, m_Allocator);
        vkFreeMemory(m_Device, m_Memory, m_Allocator);

        m_Buffer = newBuffer;
        m_Memory = newMemory;
        m_Size = newSize;
        return true;
    }

    void VulkanBuffer::BindMemory(uint64_t offset)
    {
        m_Offset = offset;
        VA_VULKAN_CHECK_RESULT_WARN(vkBindBufferMemory(m_Device, m_Buffer, m_Memory, m_Offset));
    }

    void* VulkanBuffer::LockMemory(
        const uint64_t offset,
        const uint64_t size,
        const VkMemoryMapFlags flags) const
    {
        void* data;
        vkMapMemory(m_Device, m_Memory, offset, size, flags, &data);
        return data;
    }

    void VulkanBuffer::UnlockMemory() const { vkUnmapMemory(m_Device, m_Memory); }

    void VulkanBuffer::CopyTo(
        const VkCommandPool pool,
        const VkFence fence,
        const VkQueue queue,
        const VkBuffer dest,
        const uint64_t destOffset,
        const uint64_t size) const
    {
        vkQueueWaitIdle(queue);

        // Create a one-time-use command buffer.
        VulkanCommandBuffer cmdBuf;
        VulkanCommandBuffer::SingleUseBegin(m_Device, pool, cmdBuf);

        // Prepare the copy command and add it to the command buffer
        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = destOffset;
        copyRegion.size = size;
        vkCmdCopyBuffer(cmdBuf.GetHandle(), m_Buffer, dest, 1, &copyRegion);

        VulkanCommandBuffer::SingleUseEnd(cmdBuf, queue, fence);
    }

    VulkanVertexBuffer::VulkanVertexBuffer(
        const VulkanRHI& rhi,
        const std::unique_ptr<VulkanDevice>& device,
        VkAllocationCallbacks* allocator,
        const std::vector<Resources::MeshVertex>& data,
        const bool bindOnCreate)
        : VulkanBuffer(
            rhi,
            device,
            allocator,
            data.size() * sizeof(Resources::MeshVertex),
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
            | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            bindOnCreate)
    {
        const auto staging = VulkanStagingBuffer(rhi, device, allocator, data);
        auto fence = VulkanFence(m_Device, m_Allocator);
        staging.CopyTo(
            device->GetGraphicsCommandPool(),
            fence.GetHandle(),
            device->GetGraphicsQueueHandle(),
            m_Buffer,
            0,
            m_Size);

        // TODO Find an async way to load data ?
        fence.Wait();
    }

    void VulkanVertexBuffer::Bind(IRenderingHardware& rhi)
    {
        auto& vkRhi = dynamic_cast<VulkanRHI&>(rhi);
        VkDeviceSize offsets = {0};
        vkCmdBindVertexBuffers(
            vkRhi.GetCurrentCommandBuffer().GetHandle(),
            0,
            1,
            &m_Buffer,
            &offsets);
    }

    void VulkanVertexBuffer::Unbind()
    {
    }

    VulkanIndexBuffer::VulkanIndexBuffer(
        const VulkanRHI& rhi,
        const std::unique_ptr<VulkanDevice>& device,
        VkAllocationCallbacks* allocator,
        const std::vector<uint32_t>& data,
        bool bindOnCreate)
        : VulkanBuffer(
            rhi,
            device,
            allocator,
            data.size() * sizeof(uint32_t),
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
            | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            bindOnCreate)
    {
        const auto staging = VulkanStagingBuffer(rhi, device, allocator, data);
        auto fence = VulkanFence(m_Device, m_Allocator);
        staging.CopyTo(
            device->GetGraphicsCommandPool(),
            fence.GetHandle(),
            device->GetGraphicsQueueHandle(),
            m_Buffer,
            0,
            m_Size);

        // TODO Find an async way to load data ?
        fence.Wait();
    }

    void VulkanIndexBuffer::Bind(IRenderingHardware& rhi)
    {
        auto& vkRhi = dynamic_cast<VulkanRHI&>(rhi);
        vkCmdBindIndexBuffer(
            vkRhi.GetCurrentCommandBuffer().GetHandle(),
            m_Buffer,
            0,
            VK_INDEX_TYPE_UINT32);
    }

    void VulkanIndexBuffer::Unbind()
    {
    }
} // namespace VoidArchitect::Platform
