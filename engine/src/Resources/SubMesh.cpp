//
// Created by Michael Desmedt on 14/06/2025.
//
#include "SubMesh.hpp"

#include "MeshData.hpp"
#include "Core/Logger.hpp"

VoidArchitect::Resources::SubMeshDescriptor::SubMeshDescriptor(
    std::string meshName,
    const MaterialHandle material,
    const uint32_t indexOffset,
    const uint32_t indexCount,
    const uint32_t vertexOffset,
    const uint32_t vertexCount)
    : name(std::move(meshName)),
      material(material),
      indexOffset(indexOffset),
      indexCount(indexCount),
      vertexOffset(vertexOffset),
      vertexCount(vertexCount)
{
}

bool VoidArchitect::Resources::SubMeshDescriptor::IsValid(const MeshData& data) const
{
    // Check if submesh is empty
    if (IsEmpty())
    {
        VA_ENGINE_WARN("[SubMesh] Submesh '{}' is empty (no vertices or indices)", name);
        return false;
    }

    // Check vertex range bounds
    if (vertexOffset + vertexCount > data.vertices.size())
    {
        VA_ENGINE_ERROR(
            "[SubMesh] Submesh '{}' vertex range ({} + {}) exceeds vertex buffer size ({}).",
            name,
            vertexOffset,
            vertexCount,
            data.vertices.size());
        return false;
    }

    // Check index range bounds
    if (indexOffset + indexCount > data.indices.size())
    {
        VA_ENGINE_ERROR(
            "[SubMesh] Submesh '{}' index range ({} + {}) exceeds index buffer size ({}).",
            name,
            vertexOffset,
            vertexCount,
            data.indices.size());
        return false;
    }

    // Verify all indices reference vertices within the submesh vertex range
    for (uint32_t i = indexOffset; i < indexOffset + indexCount; i++)
    {
        const uint32_t index = data.indices[i];
        if (index < vertexOffset || index >= vertexOffset + vertexCount)
        {
            VA_ENGINE_ERROR(
                "[SubMesh] Submesh '{}' contains index {} at position {} that references vertex outside submesh range [{}, {}].",
                name,
                index,
                i,
                vertexOffset,
                vertexOffset + vertexCount - 1);
            return false;
        }
    }

    // Check for valid material handle (basic check - could be enhanced)
    if (material == InvalidMaterialHandle)
    {
        VA_ENGINE_WARN("[SubMesh] Submesh '{}' has an invalid material handle.", name);
        return false;
    }

    return true;
}
