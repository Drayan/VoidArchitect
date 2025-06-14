//
// Created by Michael Desmedt on 31/05/2025.
//
#pragma once

namespace VoidArchitect
{
    class IBuffer;
}

namespace VoidArchitect
{
    class MeshSystem;

    namespace Platform
    {
        class IRenderingHardware;
    }

    namespace Resources
    {
        struct SubMeshDescriptor;
        class MeshData;

        using MaterialHandle = uint32_t;
        using MeshHandle = uint32_t;
        static constexpr MeshHandle InvalidMeshHandle = std::numeric_limits<uint32_t>::max();

        class IMesh
        {
            friend class VoidArchitect::MeshSystem;

        public:
            virtual ~IMesh() = default;

            virtual void UpdateSubmeshMaterial(uint32_t index, MaterialHandle newMaterial) = 0;

            [[nodiscard]] virtual IBuffer* GetVertexBuffer() = 0;
            [[nodiscard]] virtual IBuffer* GetIndexBuffer() = 0;
            [[nodiscard]] virtual uint32_t GetIndicesCount() const = 0;

            [[nodiscard]] virtual uint32_t GetSubMeshCount() const = 0;
            [[nodiscard]] virtual const SubMeshDescriptor& GetSubMesh(uint32_t index) const = 0;
            [[nodiscard]] virtual const VAArray<SubMeshDescriptor>& GetAllSubMeshes() const = 0;

            [[nodiscard]] virtual std::shared_ptr<MeshData> GetMeshData() const = 0;
            [[nodiscard]] virtual uint32_t GetDataGeneration() const = 0;

        protected:
            explicit IMesh(
                std::string name,
                std::shared_ptr<MeshData> data,
                const VAArray<SubMeshDescriptor>& submeshes);

            std::string m_Name;

            std::shared_ptr<MeshData> m_Data;
            VAArray<SubMeshDescriptor> m_Submeshes;
        };
    } // namespace Resources
} // namespace VoidArchitect
