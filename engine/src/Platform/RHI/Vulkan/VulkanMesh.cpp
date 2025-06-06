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
            const std::string& name,
            const VAArray<Resources::MeshVertex>& vertices,
            const VAArray<uint32_t>& indices)
            : IMesh(name),
              m_Allocator(allocator)
        {
            m_VertexBuffer = std::make_unique<VulkanVertexBuffer>(
                rhi, rhi.GetDeviceRef(), m_Allocator, vertices);
            m_IndexBuffer =
                std::make_unique<VulkanIndexBuffer>(rhi, rhi.GetDeviceRef(), m_Allocator, indices);
        }

        void VulkanMesh::Bind(Platform::IRenderingHardware& rhi)
        {
            m_VertexBuffer->Bind(rhi);
            m_IndexBuffer->Bind(rhi);
        }

        void VulkanMesh::Release()
        {
            m_VertexBuffer = nullptr;
            m_IndexBuffer = nullptr;
        }
    } // namespace Platform
} // namespace VoidArchitect
