//
// Created by Michael Desmedt on 01/06/2025.
//
#include "MeshSystem.hpp"

#include "Core/Logger.hpp"
#include "Platform/RHI/IRenderingHardware.hpp"
#include "Renderer/RenderCommand.hpp"

namespace VoidArchitect
{
    MeshSystem::MeshSystem()
    {
        m_Meshes.reserve(1024);
    }

    MeshSystem::~MeshSystem()
    {
        if (!m_MeshCache.empty())
        {
            VA_ENGINE_WARN(
                "[MeshSystem] Mesh cache is not empty during destruction. This may indicate a resource leak.");

            for (const auto& [uuid, mesh] : m_MeshCache)
            {
                if (const auto msh = mesh.lock())
                {
                    VA_ENGINE_WARN(
                        "[MeshSystem] Mesh '{}' with UUID {} was not released properly.",
                        msh->m_Name,
                        static_cast<uint64_t>(uuid));
                    msh->Release();
                }
            }
        }

        m_MeshCache.clear();
    }

    Resources::MeshPtr MeshSystem::LoadMesh(const std::string& name)
    {
        //TODO Implement loading mesh from a file
        return nullptr;
    }

    Resources::MeshPtr MeshSystem::CreateMesh(
        const std::string& name,
        const std::vector<Resources::MeshVertex>& vertices,
        const std::vector<uint32_t>& indices)
    {
        Resources::IMesh* mesh = Renderer::RenderCommand::GetRHIRef().CreateMesh(
            name,
            vertices,
            indices);
        if (!mesh)
        {
            VA_ENGINE_WARN("[MeshSystem] Failed to create mesh '{}'.", name);
            return nullptr;
        }

        auto meshPtr = Resources::MeshPtr(mesh, MeshDeleter{this});
        m_MeshCache[mesh->m_UUID] = meshPtr;

        const auto handle = GetFreeMeshHandle();
        mesh->m_Handle = handle;

        m_Meshes[handle] = {
            mesh->m_UUID,
            vertices.size() * sizeof(Resources::MeshVertex) + indices.size() * sizeof(uint32_t)
        };

        m_TotalMemoryUsed += m_Meshes[handle].dataSize;
        m_TotalMeshesLoaded++;

        VA_ENGINE_TRACE("[MeshSystem] Mesh '{}' created with handle {}.", name, handle);
        return meshPtr;
    }

    uint32_t MeshSystem::GetFreeMeshHandle()
    {
        // If there are free handles, return the first one
        if (!m_FreeMeshHandles.empty())
        {
            const uint32_t handle = m_FreeMeshHandles.front();
            m_FreeMeshHandles.pop();
            return handle;
        }

        // Otherwise, return the next handle and increment it
        if (m_NextMeshHandle >= m_Meshes.size())
        {
            m_Meshes.resize(m_NextMeshHandle + 1);
        }
        return m_NextMeshHandle++;
    }

    void MeshSystem::ReleaseMesh(const Resources::IMesh* mesh)
    {
        if (const auto it = m_MeshCache.find(mesh->m_UUID); it != m_MeshCache.end())
        {
            VA_ENGINE_TRACE("[MeshSystem] Releasing mesh '{}'.", mesh->m_Name);
            // Release the mesh handle
            m_FreeMeshHandles.push(mesh->m_Handle);
            const auto size = m_Meshes[mesh->m_Handle].dataSize;
            m_Meshes[mesh->m_Handle] = {InvalidUUID, 0};
            m_MeshCache.erase(it);

            m_TotalMemoryUsed -= size;
            m_TotalMeshesLoaded--;
        }
    }

    void MeshSystem::MeshDeleter::operator()(const Resources::IMesh* mesh) const
    {
        system->ReleaseMesh(mesh);
        delete mesh;
    }
} // VoidArchitect
