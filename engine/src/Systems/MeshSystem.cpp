//
// Created by Michael Desmedt on 01/06/2025.
//
#include "MeshSystem.hpp"

#include "Core/Logger.hpp"
#include "Core/Math/Constants.hpp"
#include "Platform/RHI/IRenderingHardware.hpp"
#include "Renderer/RenderSystem.hpp"

namespace VoidArchitect
{
    MeshSystem::MeshSystem() { m_Meshes.reserve(1024); }

    Resources::MeshHandle MeshSystem::LoadMesh(const std::string& name)
    {
        // TODO Implement loading mesh from a file
        return Resources::InvalidMeshHandle;
    }

    Resources::MeshHandle MeshSystem::LoadMesh(
        const std::string& name,
        const VAArray<Resources::MeshVertex>& vertices,
        const VAArray<uint32_t>& indices)
    {
        //TODO Implement loading mesh from datas
        return Resources::InvalidMeshHandle;
    }

    uint32_t MeshSystem::GetIndexCountFor(const Resources::MeshHandle handle) const
    {
        return m_Meshes[handle]->GetIndicesCount();
    }

    Resources::MeshHandle MeshSystem::GetHandleFor(
        const std::string& name,
        const VAArray<Resources::MeshVertex>& vertices,
        const VAArray<uint32_t>& indices)
    {
        // Check if the mesh already exists
        for (uint32_t i = 0; i < m_Meshes.size(); i++)
        {
            if (m_Meshes[i]->m_Name == name) return i;
        }

        // This is the first time the system is asked a handle for this Mesh.
        const auto meshPtr = CreateMesh(name, vertices, indices);
        const auto handle = GetFreeMeshHandle();
        m_Meshes[handle] = std::unique_ptr<Resources::IMesh>(meshPtr);

        return handle;
    }

    Resources::IMesh* MeshSystem::CreateMesh(
        const std::string& name,
        const VAArray<Resources::MeshVertex>& vertices,
        const VAArray<uint32_t>& indices)
    {
        Resources::IMesh* mesh = Renderer::g_RenderSystem->GetRHI()->CreateMesh(
            name,
            vertices,
            indices);
        if (!mesh)
        {
            VA_ENGINE_WARN("[MeshSystem] Failed to create mesh '{}'.", name);
            return nullptr;
        }

        VA_ENGINE_TRACE("[MeshSystem] Created mesh '{}'.", name);
        return mesh;
    }

    Resources::MeshHandle MeshSystem::CreateSphere(
        const std::string& name,
        const float radius,
        const uint32_t latitudeBands,
        const uint32_t longitudeBands)
    {
        VAArray<Resources::MeshVertex> vertices;
        VAArray<uint32_t> indices;

        for (uint32_t lat = 0; lat <= latitudeBands; ++lat)
        {
            const float theta = static_cast<float>(lat) * Math::PI / static_cast<float>(
                latitudeBands);
            const float sinTheta = std::sin(theta);
            const float cosTheta = std::cos(theta);

            for (uint32_t lon = 0; lon <= longitudeBands; ++lon)
            {
                const float phi = static_cast<float>(lon) * Math::PI * 2.0f / static_cast<float>(
                    longitudeBands);
                const float sinPhi = std::sin(phi);
                const float cosPhi = std::cos(phi);

                // Position
                const Math::Vec3 position(
                    radius * sinTheta * cosPhi,
                    radius * cosTheta,
                    radius * sinTheta * sinPhi);

                Math::Vec3 normal = position;
                normal.Normalize();

                // UV0
                const Math::Vec2 uv0(
                    static_cast<float>(lon) / static_cast<float>(longitudeBands),
                    static_cast<float>(lat) / static_cast<float>(latitudeBands));

                vertices.push_back({position, normal, uv0});
            }
        }

        for (uint32_t lat = 0; lat < latitudeBands; ++lat)
        {
            for (uint32_t lon = 0; lon < longitudeBands; ++lon)
            {
                const uint32_t first = (lat * (longitudeBands + 1)) + lon;
                const uint32_t second = first + longitudeBands + 1;

                indices.push_back(first);
                indices.push_back(first + 1);
                indices.push_back(second);

                indices.push_back(first + 1);
                indices.push_back(second + 1);
                indices.push_back(second);
            }
        }

        GenerateTangents(vertices, indices);
        return GetHandleFor(name, vertices, indices);
    }

    Resources::MeshHandle MeshSystem::CreateCube(const std::string& name, const float size)
    {
        const float halfSize = size * 0.5f;

        VAArray<Resources::MeshVertex> vertices;
        VAArray<uint32_t> indices;

        // Front face
        vertices.push_back(
            {
                Math::Vec3(-halfSize, -halfSize, halfSize),
                Math::Vec3::Back(),
                Math::Vec2(0.0f, 0.0f)
            });
        vertices.push_back(
            {
                Math::Vec3(halfSize, -halfSize, halfSize),
                Math::Vec3::Back(),
                Math::Vec2(1.0f, 0.0f)
            });
        vertices.push_back(
            {Math::Vec3(halfSize, halfSize, halfSize), Math::Vec3::Back(), Math::Vec2(1.0f, 1.0f)});
        vertices.push_back(
            {
                Math::Vec3(-halfSize, halfSize, halfSize),
                Math::Vec3::Back(),
                Math::Vec2(0.0f, 1.0f)
            });

        // Back face
        vertices.push_back(
            {
                Math::Vec3(halfSize, -halfSize, -halfSize),
                Math::Vec3::Forward(),
                Math::Vec2(0.0f, 0.0f)
            });
        vertices.push_back(
            {
                Math::Vec3(-halfSize, -halfSize, -halfSize),
                Math::Vec3::Forward(),
                Math::Vec2(1.0f, 0.0f)
            });
        vertices.push_back(
            {
                Math::Vec3(-halfSize, halfSize, -halfSize),
                Math::Vec3::Forward(),
                Math::Vec2(1.0f, 1.0f)
            });
        vertices.push_back(
            {
                Math::Vec3(halfSize, halfSize, -halfSize),
                Math::Vec3::Forward(),
                Math::Vec2(0.0f, 1.0f)
            });

        // Right face
        vertices.push_back(
            {
                Math::Vec3(halfSize, -halfSize, halfSize),
                Math::Vec3::Right(),
                Math::Vec2(0.0f, 0.0f)
            });
        vertices.push_back(
            {
                Math::Vec3(halfSize, -halfSize, -halfSize),
                Math::Vec3::Right(),
                Math::Vec2(1.0f, 0.0f)
            });
        vertices.push_back(
            {
                Math::Vec3(halfSize, halfSize, -halfSize),
                Math::Vec3::Right(),
                Math::Vec2(1.0f, 1.0f)
            });
        vertices.push_back(
            {
                Math::Vec3(halfSize, halfSize, halfSize),
                Math::Vec3::Right(),
                Math::Vec2(0.0f, 1.0f)
            });

        // Left face
        vertices.push_back(
            {
                Math::Vec3(-halfSize, -halfSize, -halfSize),
                Math::Vec3::Left(),
                Math::Vec2(0.0f, 0.0f)
            });
        vertices.push_back(
            {
                Math::Vec3(-halfSize, -halfSize, halfSize),
                Math::Vec3::Left(),
                Math::Vec2(1.0f, 0.0f)
            });
        vertices.push_back(
            {
                Math::Vec3(-halfSize, halfSize, halfSize),
                Math::Vec3::Left(),
                Math::Vec2(1.0f, 1.0f)
            });
        vertices.push_back(
            {
                Math::Vec3(-halfSize, halfSize, -halfSize),
                Math::Vec3::Left(),
                Math::Vec2(0.0f, 1.0f)
            });

        // Up face
        vertices.push_back(
            {Math::Vec3(-halfSize, halfSize, halfSize), Math::Vec3::Up(), Math::Vec2(0.0f, 0.0f)});
        vertices.push_back(
            {Math::Vec3(halfSize, halfSize, halfSize), Math::Vec3::Up(), Math::Vec2(1.0f, 0.0f)});
        vertices.push_back(
            {Math::Vec3(halfSize, halfSize, -halfSize), Math::Vec3::Up(), Math::Vec2(1.0f, 1.0f)});
        vertices.push_back(
            {Math::Vec3(-halfSize, halfSize, -halfSize), Math::Vec3::Up(), Math::Vec2(0.0f, 1.0f)});

        // Down face
        vertices.push_back(
            {
                Math::Vec3(-halfSize, -halfSize, -halfSize),
                Math::Vec3::Down(),
                Math::Vec2(0.0f, 0.0f)
            });
        vertices.push_back(
            {
                Math::Vec3(halfSize, -halfSize, -halfSize),
                Math::Vec3::Down(),
                Math::Vec2(1.0f, 0.0f)
            });
        vertices.push_back(
            {
                Math::Vec3(halfSize, -halfSize, halfSize),
                Math::Vec3::Down(),
                Math::Vec2(1.0f, 1.0f)
            });
        vertices.push_back(
            {
                Math::Vec3(-halfSize, -halfSize, halfSize),
                Math::Vec3::Down(),
                Math::Vec2(0.0f, 1.0f)
            });

        for (uint32_t face = 0; face < 6; ++face)
        {
            const uint32_t baseIndex = face * 4;

            indices.push_back(baseIndex);
            indices.push_back(baseIndex + 1);
            indices.push_back(baseIndex + 2);

            indices.push_back(baseIndex);
            indices.push_back(baseIndex + 2);
            indices.push_back(baseIndex + 3);
        }

        GenerateTangents(vertices, indices);
        return GetHandleFor(name, vertices, indices);
    }

    Resources::MeshHandle MeshSystem::CreateQuad(
        const std::string& name,
        const float width,
        const float height)
    {
        const float halfWidth = width * 0.5f;
        const float halfHeight = height * 0.5f;

        VAArray<Resources::MeshVertex> vertices{
            {Math::Vec3(-halfWidth, -halfHeight, 0.0f), Math::Vec3::Back(), Math::Vec2(0.0f, 0.0f)},
            {Math::Vec3(halfWidth, -halfHeight, 0.0f), Math::Vec3::Back(), Math::Vec2(1.0f, 0.0f)},
            {Math::Vec3(halfWidth, halfHeight, 0.0f), Math::Vec3::Back(), Math::Vec2(1.0f, 1.0f)},
            {Math::Vec3(-halfWidth, halfHeight, 0.0f), Math::Vec3::Back(), Math::Vec2(0.0f, 1.0f)}
        };
        const VAArray<uint32_t> indices{0, 1, 2, 2, 3, 0};

        GenerateTangents(vertices, indices);
        return GetHandleFor(name, vertices, indices);
    }

    Resources::MeshHandle MeshSystem::CreatePlane(
        const std::string& name,
        float width,
        float height,
        const Math::Vec3& normal,
        uint32_t widthSegments,
        uint32_t heightSegments)
    {
        VAArray<Resources::MeshVertex> vertices;
        VAArray<uint32_t> indices;

        auto n = normal;
        n.Normalize();

        Math::Vec3 tangent, bitangent;

        auto reference = Math::Vec3::Up();
        if (std::abs(Math::Vec3::Cross(n, reference).X()) < Math::EPSILON && std::abs(
            Math::Vec3::Cross(n, reference).Y()) < Math::EPSILON && std::abs(
            Math::Vec3::Cross(n, reference).Z()) < Math::EPSILON)
        {
            reference = Math::Vec3::Right();
        }

        tangent = Math::Vec3::Cross(n, reference);
        tangent.Normalize();

        bitangent = Math::Vec3::Cross(n, tangent);
        bitangent.Normalize();

        for (uint32_t y = 0; y <= heightSegments; ++y)
        {
            const float v = static_cast<float>(y) / static_cast<float>(heightSegments);
            for (uint32_t x = 0; x <= widthSegments; ++x)
            {
                const float u = static_cast<float>(x) / static_cast<float>(widthSegments);

                const float localX = (u - 0.5f) * width;
                const float localY = (v - 0.5f) * height;

                const auto position = tangent * localX + bitangent * localY;
                const auto uv = Math::Vec2(u, v);

                vertices.push_back({position, normal, uv});
            }
        }

        for (uint32_t y = 0; y < heightSegments; ++y)
        {
            for (uint32_t x = 0; x < widthSegments; ++x)
            {
                const uint32_t topLeft = (y * (widthSegments + 1)) + x;
                const uint32_t topRight = topLeft + 1;
                const uint32_t bottomLeft = ((y + 1) * (widthSegments + 1)) + x;
                const uint32_t bottomRight = bottomLeft + 1;

                indices.push_back(topLeft);
                indices.push_back(bottomLeft);
                indices.push_back(topRight);

                indices.push_back(topRight);
                indices.push_back(bottomLeft);
                indices.push_back(bottomRight);
            }
        }

        GenerateTangents(vertices, indices);
        return GetHandleFor(name, vertices, indices);
    }

    void MeshSystem::GenerateNormals(
        VAArray<Resources::MeshVertex>& vertices,
        const VAArray<uint32_t>& indices)
    {
        for (uint32_t i = 0; i < indices.size(); i += 3)
        {
            const uint32_t index0 = indices[i + 0];
            const uint32_t index1 = indices[i + 1];
            const uint32_t index2 = indices[i + 2];

            const Math::Vec3 edge0 = vertices[index1].Position - vertices[index0].Position;
            const Math::Vec3 edge1 = vertices[index2].Position - vertices[index0].Position;

            auto normal = Math::Vec3::Cross(edge0, edge1);
            normal.Normalize();

            // NOTE: This just generate a face normal, smoothing could be done in a separate pass.
            vertices[index0].Normal = normal;
            vertices[index1].Normal = normal;
            vertices[index2].Normal = normal;
        }
    }

    void MeshSystem::GenerateTangents(
        VAArray<Resources::MeshVertex>& vertices,
        const VAArray<uint32_t>& indices)
    {
        for (uint32_t i = 0; i < indices.size(); i += 3)
        {
            const uint32_t index0 = indices[i + 0];
            const uint32_t index1 = indices[i + 1];
            const uint32_t index2 = indices[i + 2];

            const Math::Vec3 edge0 = vertices[index1].Position - vertices[index0].Position;
            const Math::Vec3 edge1 = vertices[index2].Position - vertices[index0].Position;

            const auto deltaU0 = vertices[index1].UV0.X() - vertices[index0].UV0.X();
            const auto deltaU1 = vertices[index2].UV0.X() - vertices[index0].UV0.X();
            const auto deltaV0 = vertices[index1].UV0.Y() - vertices[index0].UV0.Y();
            const auto deltaV1 = vertices[index2].UV0.Y() - vertices[index0].UV0.Y();

            const auto dividend = (deltaU0 * deltaV1) - (deltaU1 * deltaV0);
            const auto fc = 1.0f / dividend;

            auto tangent = (edge0 * deltaV1) - (edge1 * deltaV0);
            tangent *= fc;
            tangent.Normalize();

            // Inverse tangent is normal are flipped, used in shader.
            const auto sx = deltaU0;
            const auto sy = deltaU1;
            const auto tx = deltaV0;
            const auto ty = deltaV1;
            const auto handedness = ((tx * sy - ty * sx) < 0.0f) ? -1.0f : 1.0f;
            const auto t4 = Math::Vec4(tangent, handedness);

            vertices[index0].Tangent = t4;
            vertices[index1].Tangent = t4;
            vertices[index2].Tangent = t4;
        }
    }

    Resources::IMesh* MeshSystem::GetPointerFor(const Resources::MeshHandle handle) const
    {
        return m_Meshes[handle].get();
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
    }
} // namespace VoidArchitect
