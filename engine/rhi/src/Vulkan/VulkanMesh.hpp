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
                const std::shared_ptr<Resources::MeshData>& data,
                const VAArray<Resources::SubMeshDescriptor>& submeshes);
            ~VulkanMesh() override = default;

            void UpdateSubmeshMaterial(
                uint32_t index,
                Resources::MaterialHandle newMaterial) override;

            IBuffer* GetVertexBuffer() override
            {
                UpdateGPUBuffersIfNeeded();
                return m_VertexBuffer.get();
            };

            IBuffer* GetIndexBuffer() override
            {
                UpdateGPUBuffersIfNeeded();
                return m_IndexBuffer.get();
            };
            uint32_t GetIndicesCount() const override { return m_IndexBuffer->GetCount(); }

            [[nodiscard]] const Resources::SubMeshDescriptor&
            GetSubMesh(uint32_t index) const override;

            [[nodiscard]] const VAArray<Resources::SubMeshDescriptor>&
            GetAllSubMeshes() const override { return m_Submeshes; };

            [[nodiscard]] std::shared_ptr<Resources::MeshData> GetMeshData() const override
            {
                return m_Data;
            };
            [[nodiscard]] uint32_t GetDataGeneration() const override;
            [[nodiscard]] uint32_t GetSubMeshCount() const override;

        private:
            mutable uint32_t m_LastKnownGeneration = 0;

            void UpdateGPUBuffersIfNeeded();
            void RecreateBuffers();
            void InitiliazeFromData();

            const std::unique_ptr<VulkanDevice>& m_Device;
            VkAllocationCallbacks* m_Allocator;

            std::unique_ptr<VulkanBuffer> m_VertexBuffer;
            std::unique_ptr<VulkanBuffer> m_IndexBuffer;
        };
    } // namespace Platform
} // namespace VoidArchitect
