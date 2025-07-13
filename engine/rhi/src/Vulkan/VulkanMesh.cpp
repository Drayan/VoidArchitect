//
// Created by Michael Desmedt on 31/05/2025.
//
#include "VulkanMesh.hpp"

#include "VulkanRhi.hpp"
#include "Resources/MeshData.hpp"
#include "Resources/SubMesh.hpp"

namespace VoidArchitect::Platform
{
    VulkanMesh::VulkanMesh(
        const std::unique_ptr<VulkanDevice>& device,
        VkAllocationCallbacks* allocator,
        const std::string& name,
        const std::shared_ptr<Resources::MeshData>& data,
        const VAArray<Resources::SubMeshDescriptor>& submeshes)
        : IMesh(name, data, submeshes),
          m_Device(device),
          m_Allocator(allocator)
    {
        InitiliazeFromData();
        m_LastKnownGeneration = m_Data->GetGeneration();
    }

    void VulkanMesh::UpdateSubmeshMaterial(const uint32_t index, MaterialHandle newMaterial)
    {
        VA_ENGINE_ASSERT(index < m_Submeshes.size(), "SubMesh index is out of bounds.");
        m_Submeshes[index].material = newMaterial;

        VA_ENGINE_TRACE(
            "[VulkanMesh] Updated submesh '{}' of mesh '{}' to material handle {}.",
            m_Submeshes[index].name,
            m_Name,
            newMaterial);
    }

    const Resources::SubMeshDescriptor& VulkanMesh::GetSubMesh(uint32_t index) const
    {
        VA_ENGINE_ASSERT(index < m_Submeshes.size(), "SubMesh index is out of bounds.");
        return m_Submeshes[index];
    }

    uint32_t VulkanMesh::GetDataGeneration() const
    {
        return m_Data->GetGeneration();
    }

    uint32_t VulkanMesh::GetSubMeshCount() const
    {
        return static_cast<uint32_t>(m_Submeshes.size());
    }

    void VulkanMesh::UpdateGPUBuffersIfNeeded()
    {
        const uint32_t currentGeneration = m_Data->GetGeneration();
        if (m_LastKnownGeneration == currentGeneration) return;

        VA_ENGINE_TRACE(
            "[VulkanMesh] Mesh '{}' data changed (generation {} -> {}), updating GPU buffers.",
            m_Name,
            m_LastKnownGeneration,
            currentGeneration);

        RecreateBuffers();
        m_LastKnownGeneration = currentGeneration;
    }

    void VulkanMesh::RecreateBuffers()
    {
        m_VertexBuffer = std::make_unique<VulkanVertexBuffer>(
            m_Device,
            m_Allocator,
            m_Data->vertices);

        m_IndexBuffer = std::make_unique<VulkanIndexBuffer>(m_Device, m_Allocator, m_Data->indices);

        VA_ENGINE_TRACE(
            "[VulkanMesh] GPU buffers recreated for mesh '{}' with {} submeshes (vertices: {}, indices: {}).",
            m_Name,
            m_Submeshes.size(),
            m_Data->vertices.size(),
            m_Data->indices.size());
    }

    void VulkanMesh::InitiliazeFromData()
    {
        VA_ENGINE_ASSERT(m_Data && !m_Data->IsEmpty(), "Invalid mesh data provided");

        m_VertexBuffer = std::make_unique<VulkanVertexBuffer>(
            m_Device,
            m_Allocator,
            m_Data->vertices);
        m_IndexBuffer = std::make_unique<VulkanIndexBuffer>(m_Device, m_Allocator, m_Data->indices);

        VA_ENGINE_TRACE(
            "[VulkanMesh] GPU buffers initialized for mesh '{}' with {} submeshes (vertices: {}, indices: {}).",
            m_Name,
            m_Submeshes.size(),
            m_Data->vertices.size(),
            m_Data->indices.size());
    }
}
