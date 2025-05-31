//
// Created by Michael Desmedt on 31/05/2025.
//
#pragma once
#include "VulkanBuffer.hpp"
#include "Resources/Mesh.hpp"

namespace VoidArchitect
{
    namespace Platform
    {
        class VulkanMesh : public Resources::IMesh
        {
        public:
            VulkanMesh(
                VulkanRHI& rhi,
                VkAllocationCallbacks* allocator,
                const std::vector<Resources::MeshVertex>& vertices,
                const std::vector<uint32_t>& indices);
            ~VulkanMesh() override = default;

            void Bind(Platform::IRenderingHardware& rhi) override;

            uint32_t GetIndicesCount() override { return m_IndexBuffer->GetCount(); }

        private:
            VkAllocationCallbacks* m_Allocator;

            std::unique_ptr<VulkanBuffer> m_VertexBuffer;
            std::unique_ptr<VulkanBuffer> m_IndexBuffer;
        };
    } // Platform
} // VoidArchitect
