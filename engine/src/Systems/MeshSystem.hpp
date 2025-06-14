//
// Created by Michael Desmedt on 01/06/2025.
//
#pragma once
#include "Resources/Mesh.hpp"
#include "Resources/MeshData.hpp"
#include "Resources/SubMesh.hpp"

namespace VoidArchitect
{
    namespace Resources
    {
        struct MeshVertex;
    }

    class MeshSystem
    {
    public:
        MeshSystem();
        ~MeshSystem() = default;

        MeshSystem(const MeshSystem&) = delete;
        MeshSystem& operator=(const MeshSystem&) = delete;

        Resources::MeshHandle GetHandleFor(
            const std::string& name,
            const VAArray<Resources::MeshVertex>& vertices = {},
            const VAArray<uint32_t>& indices = {},
            const VAArray<Resources::SubMeshDescriptor>& submeshes = {});

        Resources::MeshHandle LoadMesh(const std::string& name);

        void AddSubMeshTo(
            Resources::MeshHandle handle,
            const std::string& submeshName,
            MaterialHandle material,
            const VAArray<Resources::MeshVertex>& vertices,
            const VAArray<uint32_t>& indices) const;
        void RemoveSubMeshFrom(Resources::MeshHandle handle, uint32_t submeshIndex) const;

        void UpdateSubMeshMaterial(
            Resources::MeshHandle handle,
            uint32_t submeshIndex,
            MaterialHandle material) const;

        [[nodiscard]] uint32_t GetIndexCountFor(Resources::MeshHandle handle) const;
        [[nodiscard]] uint32_t GetSubMeshCountFor(Resources::MeshHandle handle) const;
        [[nodiscard]] const Resources::SubMeshDescriptor& GetSubMesh(
            Resources::MeshHandle handle,
            uint32_t submeshIndex) const;
        [[nodiscard]] MaterialHandle GetSubMeshMaterial(
            Resources::MeshHandle handle,
            uint32_t submeshIndex) const;

        //==========================================================================================
        // Basic shape procedural generators
        //==========================================================================================

        Resources::MeshHandle CreateSphere(
            const std::string& name,
            float radius = 0.5f,
            uint32_t latitudeBands = 8,
            uint32_t longitudeBands = 8);
        Resources::MeshHandle CreateCube(
            const std::string& name,
            const std::string& material = "DefaultMaterial",
            float size = 1.0f);
        Resources::MeshHandle CreateQuad(
            const std::string& name,
            float width = 1.0f,
            float height = 1.0f);
        Resources::MeshHandle CreatePlane(
            const std::string& name,
            float width = 1.0f,
            float height = 1.0f,
            const Math::Vec3& normal = Math::Vec3::Up(),
            uint32_t widthSegments = 1,
            uint32_t heightSegments = 1);

        //==========================================================================================
        // Math helpers specialized for meshes
        //==========================================================================================

        static void GenerateNormals(
            VAArray<Resources::MeshVertex>& vertices,
            const VAArray<uint32_t>& indices);
        static void GenerateTangents(
            VAArray<Resources::MeshVertex>& vertices,
            const VAArray<uint32_t>& indices);

        [[nodiscard]] Resources::IMesh* GetPointerFor(Resources::MeshHandle handle) const;

    private:
        uint32_t GetFreeMeshHandle();
        static Resources::IMesh* CreateMesh(
            const std::string& name,
            std::shared_ptr<Resources::MeshData> data,
            const VAArray<Resources::SubMeshDescriptor>& submeshes);

        std::queue<Resources::MeshHandle> m_FreeMeshHandles;
        Resources::MeshHandle m_NextMeshHandle = 0;

        VAArray<std::unique_ptr<Resources::IMesh>> m_Meshes;
    };

    inline std::unique_ptr<MeshSystem> g_MeshSystem;
} // namespace VoidArchitect
