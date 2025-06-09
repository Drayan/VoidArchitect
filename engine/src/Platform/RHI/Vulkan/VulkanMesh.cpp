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
            const std::unique_ptr<VulkanDevice>& device,
            VkAllocationCallbacks* allocator,
            const std::string& name,
            const VAArray<Resources::MeshVertex>& vertices,
            const VAArray<uint32_t>& indices)
            : IMesh(name),
              m_Device(device),
              m_Allocator(allocator)
        {
            m_VertexBuffer = std::make_unique<VulkanVertexBuffer>(
                m_Device,
                m_Allocator,
                vertices);
            m_IndexBuffer =
                std::make_unique<VulkanIndexBuffer>(m_Device, m_Allocator, indices);
        }

        void VulkanMesh::Bind(IRenderingHardware& rhi)
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
