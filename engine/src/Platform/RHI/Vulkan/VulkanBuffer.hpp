//
// Created by Michael Desmedt on 19/05/2025.
//
#pragma once

#include <vulkan/vulkan.h>

#include "VulkanUtils.hpp"

namespace VoidArchitect::Platform
{
    class VulkanRHI;
    class VulkanDevice;

    class VulkanBuffer
    {
    public:
        VulkanBuffer(
            const VulkanRHI& rhi,
            const std::unique_ptr<VulkanDevice>& device,
            VkAllocationCallbacks* allocator,
            uint64_t size,
            VkBufferUsageFlags usage,
            VkMemoryPropertyFlags memProperties,
            bool bindOnCreate = true);
        VulkanBuffer(VulkanBuffer&& other) noexcept;
        VulkanBuffer(const VulkanBuffer& other) = delete;
        ~VulkanBuffer();

        VulkanBuffer& operator=(VulkanBuffer&& other) noexcept;
        VulkanBuffer& operator=(const VulkanBuffer& other) = delete;

        bool Resize(const VulkanRHI& rhi, uint64_t newSize, VkQueue queue, VkCommandPool pool);

        void Bind(uint64_t offset);

        void* LockMemory(
            const uint64_t offset,
            const uint64_t size,
            const VkMemoryMapFlags flags) const;
        void UnlockMemory() const;

        template <typename T>
        void LoadData(std::vector<T> srcData)
        {
            auto outData = LockMemory(0, sizeof(T) * srcData.size(), 0);
            memcpy(outData, srcData.data(), sizeof(T) * srcData.size());
            UnlockMemory();
        }

        void CopyTo(
            VkCommandPool pool,
            VkFence fence,
            VkQueue queue,
            VkBuffer dest,
            uint64_t destOffset,
            uint64_t size) const;

        VkBuffer GetHandle() const { return m_Buffer; }

    private:
        void InvalidateResources();

        VkDevice m_Device;
        VkAllocationCallbacks* m_Allocator;

        VkBuffer m_Buffer;
        VkDeviceMemory m_Memory;
        bool m_Locked = false;

        uint64_t m_Offset;
        uint64_t m_Size;
        VkBufferUsageFlags m_Usage;
        VkMemoryPropertyFlags m_MemoryProperties;
    };
} // VoidArchitect
