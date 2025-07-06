//
// Created by Michael Desmedt on 31/05/2025.
//
#pragma once
#include "Core/Handle.hpp"

namespace VoidArchitect
{
    class MeshSystem;
    class IBuffer;
    struct MeshNode;

    namespace Platform
    {
        class IRenderingHardware;
    }

    namespace Resources
    {
        struct SubMeshDescriptor;
        class MeshData;

        using MaterialHandle = uint32_t;

        /// @brief Handle type for mesh resources
        using MeshHandle = Handle<MeshNode>;
        static constexpr MeshHandle InvalidMeshHandle = MeshHandle::Invalid();

        /// @brief Loading state for asynchronous mesh operations
        ///
        /// Tracks the current state of mesh loading to enable non-blocking
        /// mesh requests and proper synchronization with the job system.
        /// State transitions: Unloaded -> Loading -> Loaded / Failed.
        enum class MeshLoadingState : uint8_t
        {
            Unloaded, ///< Mesh not yet requested or loading not started
            Loading, ///< Asynchronous loading job is in progress
            Loaded, ///< Mesh successfully loaded and available for use
            Failed, ///< Loading failed, error mesh is being used as fallback
        };

        class IMesh
        {
            friend class VoidArchitect::MeshSystem;

        public:
            virtual ~IMesh() = default;

            virtual void UpdateSubmeshMaterial(uint32_t index, MaterialHandle newMaterial) = 0;

            /// @brief Check if mesh resources have changed since last update
            /// @return true if resources have changed and require rebinding
            ///
            /// NOTE: Currently not used as meshes are bound directly per-frame.
            /// Added for interface consistency and future mesh binding managers
            /// (e.g. for compute shaders, indirect drawing, instancing).
            virtual bool HasResourcesChanged() const { return true; }

            /// @brief Mark mesh resources as updated after binding operations
            ///
            /// NOTE: Currently not used as meshes are bound directly per-frame.
            /// Added for interface consistency and future mesh binding managers.
            virtual void MarkResourcesUpdated()
            {
            }

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
