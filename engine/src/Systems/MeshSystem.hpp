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
        ~MeshSystem();

        Resources::MeshPtr LoadMesh(const std::string& name);
        Resources::MeshPtr CreateMesh(
            const std::string& name,
            const std::vector<Resources::MeshVertex>& vertices,
            const std::vector<uint32_t>& indices);
        Resources::MeshPtr CreateSphere(
            const std::string& name,
            float radius = 0.5f,
            uint32_t latitudeBands = 8,
            uint32_t longitudeBands = 8);
        Resources::MeshPtr CreateCube(const std::string& name, float size = 1.0f);
        Resources::MeshPtr CreatePlane(
            const std::string& name,
            float width = 1.0f,
            float height = 1.0f,
            const Math::Vec3& normal = Math::Vec3::Up(),
            uint32_t widthSegments = 1,
            uint32_t heightSegments = 1);

    private:
        uint32_t GetFreeMeshHandle();
        void ReleaseMesh(const Resources::IMesh* mesh);

        struct MeshDeleter
        {
            MeshSystem* system;
            void operator()(const Resources::IMesh* mesh) const;
        };

        struct MeshData
        {
            UUID uuid;
            size_t dataSize = 0;
        };

        std::queue<uint32_t> m_FreeMeshHandles;
        uint32_t m_NextMeshHandle = 0;

        std::vector<MeshData> m_Meshes;
        std::unordered_map<UUID, std::weak_ptr<Resources::IMesh>> m_MeshCache;

        size_t m_TotalMemoryUsed = 0;
        size_t m_TotalMeshesLoaded = 0;
    };

    inline std::unique_ptr<MeshSystem> g_MeshSystem;
} // namespace VoidArchitect
