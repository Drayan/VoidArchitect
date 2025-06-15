//
// Created by Michael Desmedt on 01/06/2025.
//
#include "MeshSystem.hpp"

#include "MaterialSystem.hpp"
#include "ResourceSystem.hpp"
#include "Core/Logger.hpp"
#include "Core/Math/Constants.hpp"
#include "Platform/RHI/IRenderingHardware.hpp"
#include "Renderer/RenderSystem.hpp"
#include "Resources/Loaders/MeshLoader.hpp"

namespace VoidArchitect
{
    MeshSystem::MeshSystem() { m_Meshes.reserve(1024); }

    Resources::MeshHandle MeshSystem::LoadMesh(const std::string& name)
    {
        const auto meshData = g_ResourceSystem->LoadResource<
            Resources::Loaders::MeshDataDefinition>(ResourceType::Mesh, name);

        if (!meshData || (meshData->GetVertices().empty() || meshData->GetIndices().empty()))
        {
            VA_ENGINE_ERROR("[MeshSystem] Failed to load mesh '{}'.", name);
            return Resources::InvalidMeshHandle;
        }

        return GetHandleFor(
            name,
            meshData->GetVertices(),
            meshData->GetIndices(),
            meshData->GetSubmeshes());
    }

    uint32_t MeshSystem::GetIndexCountFor(const Resources::MeshHandle handle) const
    {
        return m_Meshes[handle]->GetIndicesCount();
    }

    uint32_t MeshSystem::GetSubMeshCountFor(const Resources::MeshHandle handle) const
    {
        VA_ENGINE_ASSERT(handle < m_Meshes.size() && m_Meshes[handle], "Invalid mesh handle");
        return m_Meshes[handle]->GetSubMeshCount();
    }

    const Resources::SubMeshDescriptor& MeshSystem::GetSubMesh(
        const Resources::MeshHandle handle,
        const uint32_t submeshIndex) const
    {
        VA_ENGINE_ASSERT(handle < m_Meshes.size() && m_Meshes[handle], "Invalid mesh handle");
        return m_Meshes[handle]->GetSubMesh(submeshIndex);
    }

    MaterialHandle MeshSystem::GetSubMeshMaterial(
        const Resources::MeshHandle handle,
        const uint32_t submeshIndex) const
    {
        VA_ENGINE_ASSERT(handle < m_Meshes.size() && m_Meshes[handle], "Invalid mesh handle");
        return m_Meshes[handle]->GetSubMesh(submeshIndex).material;
    }

    Resources::MeshHandle MeshSystem::GetHandleFor(
        const std::string& name,
        const VAArray<Resources::MeshVertex>& vertices,
        const VAArray<uint32_t>& indices,
        const VAArray<Resources::SubMeshDescriptor>& submeshes)
    {
        // Check if the mesh already exists
        for (uint32_t i = 0; i < m_Meshes.size(); i++)
        {
            if (m_Meshes[i]->m_Name == name) return i;
        }

        if (vertices.empty() && indices.empty())
        {
            // VA_ENGINE_WARN(
            //     "[MeshSystem] Mesh '{}' wasn't in the cache, and no vertices or indices were provided.",
            //     name);
            // return Resources::InvalidMeshHandle;

            return LoadMesh(name);
        }

        // If no submeshes are provided, this is a single submesh mesh, we create one.
        if (submeshes.empty())
        {
            // TODO: Let the user provide a material
            Resources::SubMeshDescriptor singleSubmesh{
                name,
                g_MaterialSystem->GetHandleForDefaultMaterial(),
                0,
                static_cast<uint32_t>(indices.size()),
                0,
                static_cast<uint32_t>(vertices.size())
            };
            return GetHandleFor(name, vertices, indices, {singleSubmesh});
        }

        // This is the first time the system is asked a handle for this Mesh.
        const auto meshData = std::make_shared<Resources::MeshData>(vertices, indices);

        // Validate all submeshes
        for (const auto& submesh : submeshes)
        {
            if (!submesh.IsValid(*meshData))
            {
                VA_ENGINE_ERROR(
                    "[MeshSystem] Submesh '{}' for mesh '{}' is invalid.",
                    submesh.name,
                    name);
                return Resources::InvalidMeshHandle;
            }
        }

        const auto meshPtr = CreateMesh(name, meshData, submeshes);
        const auto handle = GetFreeMeshHandle();
        m_Meshes[handle] = std::unique_ptr<Resources::IMesh>(meshPtr);

        VA_ENGINE_TRACE(
            "[MeshSystem] Mesh '{}' created with handle {} and {} submeshes.",
            name,
            handle,
            submeshes.size());

        return handle;
    }

    Resources::IMesh* MeshSystem::CreateMesh(
        const std::string& name,
        std::shared_ptr<Resources::MeshData> data,
        const VAArray<Resources::SubMeshDescriptor>& submeshes)
    {
        Resources::IMesh* mesh = Renderer::g_RenderSystem->GetRHI()->CreateMesh(
            name,
            data,
            submeshes);
        if (!mesh)
        {
            VA_ENGINE_WARN("[MeshSystem] Failed to create mesh '{}'.", name);
            return nullptr;
        }

        return mesh;
    }

    void MeshSystem::AddSubMeshTo(
        const Resources::MeshHandle handle,
        const std::string& submeshName,
        const MaterialHandle material,
        const VAArray<Resources::MeshVertex>& vertices,
        const VAArray<uint32_t>& indices) const
    {
        VA_ENGINE_ASSERT(handle < m_Meshes.size() && m_Meshes[handle], "Invalid mesh handle");

        auto* mesh = m_Meshes[handle].get();
        auto meshData = mesh->GetMeshData();

        const uint32_t vertexOffset = static_cast<uint32_t>(meshData->vertices.size());
        const uint32_t indexOffset = static_cast<uint32_t>(meshData->indices.size());

        // Add geometry to mesh data
        meshData->AddSubmesh(vertices, indices);

        // Create new submesh descriptor
        const Resources::SubMeshDescriptor submesh{
            submeshName,
            material,
            indexOffset,
            static_cast<uint32_t>(indices.size()),
            vertexOffset,
            static_cast<uint32_t>(vertices.size())
        };

        // Add submesh to mesh (this will trigger GPU buffer update)
        mesh->m_Submeshes.push_back(submesh);

        VA_ENGINE_TRACE("[MeshSystem] Add submesh '{}' to mesh '{}'.", submeshName, mesh->m_Name);
    }

    void MeshSystem::RemoveSubMeshFrom(
        const Resources::MeshHandle handle,
        const uint32_t submeshIndex) const
    {
        VA_ENGINE_ASSERT(handle < m_Meshes.size() && m_Meshes[handle], "Invalid mesh handle");

        auto* mesh = m_Meshes[handle].get();
        VA_ENGINE_ASSERT(submeshIndex < mesh->m_Submeshes.size(), "Submesh index out of range");

        const auto& submesh = mesh->m_Submeshes[submeshIndex];
        auto meshData = mesh->GetMeshData();

        // Remove geometry from mesh data
        meshData->RemoveSubmesh(
            submesh.vertexOffset,
            submesh.vertexCount,
            submesh.indexOffset,
            submesh.indexCount);

        // Remove submesh descriptor
        mesh->m_Submeshes.erase(mesh->m_Submeshes.begin() + submeshIndex);

        // Update offsets for subsequent submeshes
        for (uint32_t i = submeshIndex; i < mesh->m_Submeshes.size(); ++i)
        {
            auto& desc = mesh->m_Submeshes[i];
            if (desc.vertexOffset >= submesh.vertexOffset + submesh.vertexCount)
            {
                desc.vertexOffset -= submesh.vertexCount;
            }
            if (desc.indexOffset >= submesh.indexOffset + submesh.indexCount)
            {
                desc.indexOffset -= submesh.indexCount;
            }
        }

        VA_ENGINE_TRACE(
            "[MeshSystem] Removed submesh '{}' from mesh '{}'.",
            submesh.name,
            mesh->m_Name);
    }

    void MeshSystem::UpdateSubMeshMaterial(
        const Resources::MeshHandle handle,
        const uint32_t submeshIndex,
        const MaterialHandle material) const
    {
        VA_ENGINE_ASSERT(handle < m_Meshes.size() && m_Meshes[handle], "Invalid mesh handle");

        m_Meshes[handle]->UpdateSubmeshMaterial(submeshIndex, material);
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

    Resources::MeshHandle MeshSystem::CreateCube(
        const std::string& name,
        const std::string& material,
        const float size)
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

        if (material != "DefaultMaterial")
        {
            const auto materialHandle = g_MaterialSystem->GetHandleFor(material);
            const Resources::SubMeshDescriptor subMeshDescriptor{
                "Cube",
                materialHandle,
                0,
                static_cast<uint32_t>(indices.size()),
                0,
                static_cast<uint32_t>(vertices.size())
            };

            return GetHandleFor(name, vertices, indices, {subMeshDescriptor});
        }
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
            const auto handedness = ((tx * sy - ty * sx) < 0.0f) ? 1.0f : -1.0f;
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
} // namespace VoidArchitect
