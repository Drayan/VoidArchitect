//
// Created by Michael Desmedt on 14/06/2025.
//
#pragma once
#include "Material.hpp"

namespace VoidArchitect::Resources
{
    class MeshData;

    struct SubMeshDescriptor
    {
        std::string name;
        MaterialHandle material;
        uint32_t indexOffset;
        uint32_t indexCount;
        uint32_t vertexOffset;
        uint32_t vertexCount;

        SubMeshDescriptor() = default;
        SubMeshDescriptor(
            std::string meshName,
            MaterialHandle material,
            uint32_t indexOffset,
            uint32_t indexCount,
            uint32_t vertexOffset,
            uint32_t vertexCount);

        [[nodiscard]] bool IsValid(const MeshData& data) const;

        [[nodiscard]] bool IsEmpty() const { return indexCount == 0 || vertexCount == 0; }
        [[nodiscard]] uint32_t GetIndexEnd() const { return indexOffset + indexCount; }
        [[nodiscard]] uint32_t GetVertexEnd() const { return vertexOffset + vertexCount; }
    };
}
