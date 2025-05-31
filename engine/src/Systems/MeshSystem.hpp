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
} // VoidArchitect
