//
// Created by Michael Desmedt on 01/06/2025.
//
#pragma once
#include "Core/Uuid.hpp"
#include "Resources/Mesh.hpp"

namespace VoidArchitect
{
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
            const VAArray<uint32_t>& indices = {});

        Resources::MeshHandle LoadMesh(const std::string& name);
        Resources::MeshHandle LoadMesh(
            const std::string& name,
            const VAArray<Resources::MeshVertex>& vertices,
            const VAArray<uint32_t>& indices);

        Resources::MeshHandle CreateSphere(
            const std::string& name,
            float radius = 0.5f,
            uint32_t latitudeBands = 8,
            uint32_t longitudeBands = 8);
        Resources::MeshHandle CreateCube(const std::string& name, float size = 1.0f);
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

        Resources::IMesh* GetPointerFor(Resources::MeshHandle handle) const;

    private:
        uint32_t GetFreeMeshHandle();
        static Resources::IMesh* CreateMesh(
            const std::string& name,
            const VAArray<Resources::MeshVertex>& vertices,
            const VAArray<uint32_t>& indices);
        void ReleaseMesh(const Resources::IMesh* mesh);

        std::queue<Resources::MeshHandle> m_FreeMeshHandles;
        Resources::MeshHandle m_NextMeshHandle = 0;

        VAArray<std::unique_ptr<Resources::IMesh>> m_Meshes;
    };

    inline std::unique_ptr<MeshSystem> g_MeshSystem;
} // namespace VoidArchitect
