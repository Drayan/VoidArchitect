//
// Created by Michael Desmedt on 31/05/2025.
//
#include "VulkanMesh.hpp"

#include "VulkanRhi.hpp"

namespace VoidArchitect
{
    namespace Platform
    {
        VulkanMesh::VulkanMesh(
            VulkanRHI& rhi,
            VkAllocationCallbacks* allocator,
            const std::vector<Resources::MeshVertex>& vertices,
            const std::vector<uint32_t>& indices)
            : m_Allocator(allocator)
        {
            m_VertexBuffer = std::make_unique<VulkanVertexBuffer>(
                rhi,
                rhi.GetDeviceRef(),
                m_Allocator,
                vertices);
            m_IndexBuffer = std::make_unique<VulkanIndexBuffer>(
                rhi,
                rhi.GetDeviceRef(),
                m_Allocator,
                indices);
        }

        void VulkanMesh::Bind(Platform::IRenderingHardware& rhi)
        {
            m_VertexBuffer->Bind(rhi);
            m_IndexBuffer->Bind(rhi);
        }
    } // Platform
} // VoidArchitect
