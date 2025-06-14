//
// Created by Michael Desmedt on 14/06/2025.
//
#pragma once
#include "Core/Math/Vec2.hpp"
#include "Core/Math/Vec3.hpp"
#include "Core/Math/Vec4.hpp"

namespace VoidArchitect
{
    namespace Resources
    {
        struct MeshVertex
        {
            Math::Vec3 Position;
            Math::Vec3 Normal;
            Math::Vec2 UV0;
            Math::Vec4 Tangent;
        };

        class MeshData
        {
        public:
            VAArray<MeshVertex> vertices;
            VAArray<uint32_t> indices;

            MeshData() = default;
            MeshData(VAArray<MeshVertex> vertices, VAArray<uint32_t> indices);

            [[nodiscard]] uint32_t GetGeneration() const { return m_Generation; }

            void AddSubmesh(
                const VAArray<MeshVertex>& newVertices,
                const VAArray<uint32_t>& newIndices);
            void RemoveSubmesh(
                uint32_t vertexOffset,
                uint32_t vertexCount,
                uint32_t indexOffset,
                uint32_t indexCount);
            void UpdateVertices(uint32_t offset, const VAArray<MeshVertex>& newVertices);
            void UpdateIndices(uint32_t offset, const VAArray<uint32_t>& newIndices);

            void OptimizeForGPU();
            void GenerateNormals();
            void GenerateTangents();
            void RecalculateBounds();

            [[nodiscard]] bool IsEmpty() const { return vertices.empty() || indices.empty(); }

            [[nodiscard]] size_t GetVertexDataSize() const
            {
                return vertices.size() * sizeof(MeshVertex);
            }

            [[nodiscard]] size_t GetIndexDataSize() const
            {
                return indices.size() * sizeof(uint32_t);
            }

            [[nodiscard]] size_t GetTotalDataSize() const
            {
                return GetVertexDataSize() + GetIndexDataSize();
            }

        private:
            uint32_t m_Generation = 0;
        };
    } // Resources
} // VoidArchitect
