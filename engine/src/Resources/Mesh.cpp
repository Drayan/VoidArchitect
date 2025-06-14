//
// Created by Michael Desmedt on 31/05/2025.
//
#include "Mesh.hpp"
#include "SubMesh.hpp"

namespace VoidArchitect::Resources
{
    IMesh::IMesh(
        std::string name,
        std::shared_ptr<MeshData> data,
        const VAArray<SubMeshDescriptor>& submeshes)
        : m_Name(std::move(name)),
          m_Data(std::move(data)),
          m_Submeshes(submeshes)
    {
    }
} // namespace VoidArchitect::Resources
