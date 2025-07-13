//
// Created by Michael Desmedt on 19/05/2025.
//
#pragma once

#include <Interface/Buffer.hpp>

#include "VulkanUtils.hpp"

namespace VoidArchitect::Resources
{
    struct MeshVertex;
}

namespace VoidArchitect::Platform
{
    class VulkanRHI;
    class VulkanDevice;

    class VulkanBuffer : public IBuffer
    {
    public:
        VulkanBuffer(
            const std::unique_ptr<VulkanDevice>& device,
            VkAllocationCallbacks* allocator,
            uint64_t size,
            VkBufferUsageFlags usage,
            VkMemoryPropertyFlags memProperties,
            bool bindOnCreate = true);
        VulkanBuffer(VulkanBuffer&& other) noexcept;
        VulkanBuffer(const VulkanBuffer& other) = delete;
        ~VulkanBuffer() override;

        VulkanBuffer& operator=(VulkanBuffer&& other) noexcept;
        VulkanBuffer& operator=(const VulkanBuffer& other) = delete;

        // void Bind(uint64_t offset);
        void BindMemory(uint64_t offset = 0);

        void Bind(IRenderingHardware& rhi) override
        {
        }

        void Unbind() override
        {
        }

        template <typename T>
        void LoadData(VAArray<T>& data)
        {
            const auto bufData = LockMemory(0, m_Size, 0);
            memcpy(bufData, data.data(), m_Size);
            UnlockMemory();
        }

        bool Resize(
            const std::unique_ptr<VulkanDevice>& device,
            uint64_t newSize,
            VkQueue queue,
            VkCommandPool pool);
        void* LockMemory(
            const uint64_t offset,
            const uint64_t size,
            const VkMemoryMapFlags flags) const;
        void UnlockMemory() const;

        void CopyTo(
            VkCommandPool pool,
            VkFence fence,
            VkQueue queue,
            VkBuffer dest,
            uint64_t destOffset,
            uint64_t size) const;

        VkBuffer GetHandle() const { return m_Buffer; }
        uint64_t GetByteSize() const { return m_Size; }
        uint64_t GetOffset() const { return m_Offset; }
        size_t GetCount() const { return m_Size / sizeof(float); }
        bool IsLocked() const { return m_Locked; }

    protected:
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

    template <typename T>
    class VulkanStagingBuffer : public VulkanBuffer
    {
    public:
        VulkanStagingBuffer(
            const std::unique_ptr<VulkanDevice>& device,
            VkAllocationCallbacks* allocator,
            const VAArray<T>& data,
            bool bindOnCreate = true)
            : VulkanBuffer(
                device,
                allocator,
                data.size() * sizeof(T),
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
        {
            const auto bufData = LockMemory(0, m_Size, 0);
            memcpy(bufData, data.data(), m_Size);
            UnlockMemory();
        }

        void Bind(IRenderingHardware& rhi) override
        {
        }

        void Unbind() override
        {
        }
    };

    class VulkanVertexBuffer : public VulkanBuffer
    {
    public:
        VulkanVertexBuffer(
            const std::unique_ptr<VulkanDevice>& device,
            VkAllocationCallbacks* allocator,
            const VAArray<Resources::MeshVertex>& data,
            bool bindOnCreate = true);

        void Bind(IRenderingHardware& rhi) override;
        void Unbind() override;
    };

    class VulkanIndexBuffer : public VulkanBuffer
    {
    public:
        VulkanIndexBuffer(
            const std::unique_ptr<VulkanDevice>& device,
            VkAllocationCallbacks* allocator,
            const VAArray<uint32_t>& data,
            bool bindOnCreate = true);

        void Bind(IRenderingHardware& rhi) override;
        void Unbind() override;
    };
} // namespace VoidArchitect::Platform
