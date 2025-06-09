//
// Created by Michael Desmedt on 31/05/2025.
//
#pragma once
#include "Resources/Mesh.hpp"
#include "VulkanBuffer.hpp"

namespace VoidArchitect
{
    namespace Platform
    {
        class VulkanMesh : public Resources::IMesh
        {
        public:
            VulkanMesh(
                const std::unique_ptr<VulkanDevice>& device,
                VkAllocationCallbacks* allocator,
                const std::string& name,
                const VAArray<Resources::MeshVertex>& vertices,
                const VAArray<uint32_t>& indices);
            ~VulkanMesh() override = default;

            void Bind(Platform::IRenderingHardware& rhi) override;
            void Release() override;

            uint32_t GetIndicesCount() override { return m_IndexBuffer->GetCount(); }

        private:
            const std::unique_ptr<VulkanDevice>& m_Device;
            VkAllocationCallbacks* m_Allocator;

            std::unique_ptr<VulkanBuffer> m_VertexBuffer;
            std::unique_ptr<VulkanBuffer> m_IndexBuffer;
        };
    } // namespace Platform
} // namespace VoidArchitect
