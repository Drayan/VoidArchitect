//
// Created by Michael Desmedt on 14/06/2025.
//
#include "MeshData.hpp"

#include "Core/Core.hpp"
#include "Core/Logger.hpp"

namespace VoidArchitect::Resources
{
    MeshData::MeshData(VAArray<MeshVertex> vertices, VAArray<uint32_t> indices)
        : vertices(std::move(vertices)),
          indices(std::move(indices))
    {
        m_Generation++;
    }

    void MeshData::AddSubmesh(
        const VAArray<MeshVertex>& newVertices,
        const VAArray<uint32_t>& newIndices)
    {
        const auto vertexOffset = static_cast<uint32_t>(vertices.size());

        // Add vertices
        vertices.reserve(vertices.size() + newVertices.size());
        vertices.insert(vertices.end(), newVertices.begin(), newVertices.end());

        // Add indices with offset adjustment
        indices.reserve(indices.size() + newIndices.size());
        for (const auto& index : newIndices)
        {
            indices.push_back(index + vertexOffset);
        }

        m_Generation++;
    }

    void MeshData::RemoveSubmesh(
        const uint32_t vertexOffset,
        const uint32_t vertexCount,
        const uint32_t indexOffset,
        const uint32_t indexCount)
    {
        VA_ENGINE_ASSERT(
            vertexOffset + vertexCount <= vertices.size(),
            "Vertex range is out of bounds");
        VA_ENGINE_ASSERT(
            indexOffset + indexCount <= indices.size(),
            "Index range is out of bounds");

        // Remove vertices
        vertices.erase(
            vertices.begin() + vertexOffset,
            vertices.begin() + vertexOffset + vertexCount);

        // Remove indices and adjust remaining indices
        indices.erase(indices.begin() + indexOffset, indices.begin() + indexOffset + indexCount);
        for (auto& index : indices)
        {
            if (index >= vertexOffset + vertexCount) index -= vertexCount;
        }

        m_Generation++;
    }

    void MeshData::UpdateVertices(const uint32_t offset, const VAArray<MeshVertex>& newVertices)
    {
        VA_ENGINE_ASSERT(
            offset + newVertices.size() <= vertices.size(),
            "Update vertices range is out of bounds");

        std::ranges::copy(newVertices, vertices.begin() + offset);
        m_Generation++;
    }

    void MeshData::UpdateIndices(const uint32_t offset, const VAArray<uint32_t>& newIndices)
    {
        VA_ENGINE_ASSERT(
            offset + newIndices.size() <= indices.size(),
            "Update indices range is out of bounds");

        std::ranges::copy(newIndices, indices.begin() + offset);
        m_Generation++;
    }

    void MeshData::OptimizeForGPU()
    {
        //TODO: Implement vertex cache optimization
        //      This could include :
        //          - Vertex cache optimization (reorder indices for better cache coherency)
        //          - Vertex deduplication
        //          - Triangle strip conversion
        //          - Spatial locality optimization
        //          - ... ?

        m_Generation++;

        VA_ENGINE_WARN("[MeshData] OptimizeForGPU not implemented !");
    }

    void MeshData::GenerateNormals()
    {
        for (auto& vertex : vertices) vertex.Normal = Math::Vec3::Zero();

        for (uint32_t i = 0; i < indices.size(); i += 3)
        {
            const uint32_t index0 = indices[i + 0];
            const uint32_t index1 = indices[i + 1];
            const uint32_t index2 = indices[i + 2];

            const Math::Vec3 edge0 = vertices[index1].Position - vertices[index0].Position;
            const Math::Vec3 edge1 = vertices[index2].Position - vertices[index0].Position;

            const auto normal = Math::Vec3::Cross(edge0, edge1);

            vertices[index0].Normal += normal;
            vertices[index1].Normal += normal;
            vertices[index2].Normal += normal;
        }

        for (auto& vertex : vertices) vertex.Normal.Normalize();

        m_Generation++;
    }

    void MeshData::GenerateTangents()
    {
        for (auto& vertex : vertices) vertex.Tangent = Math::Vec4::Zero();

        VAArray<Math::Vec3> tangents(vertices.size(), Math::Vec3::Zero());
        VAArray<Math::Vec3> bitangents(vertices.size(), Math::Vec3::Zero());

        for (uint32_t i = 0; i < indices.size(); i += 3)
        {
            const uint32_t index0 = indices[i + 0];
            const uint32_t index1 = indices[i + 1];
            const uint32_t index2 = indices[i + 2];

            const Math::Vec3& v0 = vertices[index0].Position;
            const Math::Vec3& v1 = vertices[index1].Position;
            const Math::Vec3& v2 = vertices[index2].Position;

            const Math::Vec2& uv0 = vertices[index0].UV0;
            const Math::Vec2& uv1 = vertices[index1].UV0;
            const Math::Vec2& uv2 = vertices[index2].UV0;

            const Math::Vec3 edge1 = v1 - v0;
            const Math::Vec3 edge2 = v2 - v0;

            const Math::Vec2 dUV1 = uv1 - uv0;
            const Math::Vec2 dUV2 = uv2 - uv0;

            const float r = 1.0f / (dUV1.X() * dUV2.Y() - dUV2.X() * dUV1.Y());
            const Math::Vec3 sdir = (edge1 * dUV2.Y() - edge2 * dUV1.Y()) * r;
            const Math::Vec3 tdir = (edge2 * dUV1.X() - edge1 * dUV2.X()) * r;

            tangents[index0] += sdir;
            tangents[index1] += sdir;
            tangents[index2] += sdir;

            bitangents[index0] += tdir;
            bitangents[index1] += tdir;
            bitangents[index2] += tdir;
        }

        for (size_t i = 0; i < vertices.size(); ++i)
        {
            const Math::Vec3& n = vertices[i].Normal;
            const Math::Vec3& t = tangents[i];

            // Gram-Schmidt orthogonalize
            const Math::Vec3 orthoTangent = (t - n * Math::Vec3::Dot(n, t)).Normalize();

            // Calculate handedness
            const float handedness = (Math::Vec3::Dot(Math::Vec3::Cross(n, t), bitangents[i]) <
                                         0.0f)
                                         ? -1.0f
                                         : 1.0f;

            vertices[i].Tangent = Math::Vec4(orthoTangent, handedness);
        }

        m_Generation++;
    }

    void MeshData::RecalculateBounds()
    {
        //TODO: Implement bounding box calculation
        //  This would calculate and store AABB for culling purpose
        VA_ENGINE_WARN("[MeshData] RecalculateBounds not implemented !");
    }
}
