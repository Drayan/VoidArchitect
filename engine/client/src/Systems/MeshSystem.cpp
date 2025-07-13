//
// Created by Michael Desmedt on 01/06/2025.
//
#include "MeshSystem.hpp"

#include <VoidArchitect/Engine/Common/Logger.hpp>
#include <VoidArchitect/Engine/Common/Math/Constants.hpp>
#include <VoidArchitect/Engine/Common/Systems/Jobs/JobSystem.hpp>
#include <VoidArchitect/Engine/RHI/Interface/IRenderingHardware.hpp>

#include "MaterialSystem.hpp"
#include "ResourceSystem.hpp"
#include "Renderer/RenderSystem.hpp"
#include "Resources/Loaders/RawMeshLoader.hpp"

namespace VoidArchitect
{
    //=========================================================================
    // MeshLoadingStorage Implementation
    //=========================================================================

    void MeshLoadingStorage::StoreCompletedLoad(
        const std::string& name,
        std::shared_ptr<Resources::Loaders::MeshDataDefinition> definition)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_CompletedLoads[name] = std::move(definition);

        VA_ENGINE_TRACE("[MeshLoadingStorage] Stored completed load for mesh '{}'.", name);
    }

    std::shared_ptr<Resources::Loaders::MeshDataDefinition>
    MeshLoadingStorage::RetrieveCompletedLoad(const std::string& name)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);

        auto it = m_CompletedLoads.find(name);
        if (it == m_CompletedLoads.end())
        {
            VA_ENGINE_WARN(
                "[MeshLoadingStorage] Failed to retrieve completed load for mesh '{}'.",
                name);
            return nullptr;
        }

        auto definition = std::move(it->second);
        m_CompletedLoads.erase(it);

        VA_ENGINE_TRACE("[MeshLoadingStorage] Retrieved completed load for mesh '{}'.", name);

        return definition;
    }

    //=========================================================================
    // MeshSystem Implementation
    //=========================================================================

    MeshSystem::MeshSystem()
    {
        CreateErrorMesh();
    }

    Resources::MeshHandle MeshSystem::GetHandleFor(
        const std::string& name,
        const VAArray<Resources::MeshVertex>& vertices,
        const VAArray<uint32_t>& indices,
        const VAArray<Resources::SubMeshDescriptor>& submeshes)
    {
        // Check cache first
        if (const auto it = m_NameToHandleMap.find(name); it != m_NameToHandleMap.end())
        {
            const auto handle = it->second;

            // Verify handle is still valid (could have been freed)
            if (auto* node = m_MeshStorage.Get(handle); node)
            {
                // If loading, check if async operation completed
                if (node->state == Resources::MeshLoadingState::Loading)
                {
                    if (Jobs::g_JobSystem && Jobs::g_JobSystem->IsSignaled(node->loadingComplete))
                    {
                        auto result = Jobs::g_JobSystem->GetSyncPointStatus(node->loadingComplete);
                        if (result == Jobs::JobResult::Status::Success)
                        {
                            // Loading completed succesfully - mesh should be uploaded
                            // The state update happens in CreateMeshUploadJob
                        }
                        else
                        {
                            // Async loading failed - mark as failed
                            node->state = Resources::MeshLoadingState::Failed;
                            node->loadingComplete = Jobs::InvalidSyncPointHandle;
                            VA_ENGINE_ERROR("[MeshSystem] Failed to load mesh '{}'.", name);
                        }
                    }
                }

                return handle;
            }
            else
            {
                // Handle is invalid - remove from cache
                m_NameToHandleMap.erase(it);
                VA_ENGINE_WARN("[MeshSystem] Mesh handle for '{}' is invalid.", name);
            }
        }

        // Handle procedural mesh creation (vertices / indices provided)
        if (!vertices.empty() && !indices.empty())
        {
            return CreateProceduralMesh(name, vertices, indices, submeshes);
        }

        // Handle file-based mesh loading (no vertices / indices provided)
        if (vertices.empty() && indices.empty())
        {
            // First time request - create new mesh node and start async loading
            auto handle = CreateMeshNode(name);
            if (handle.IsValid())
            {
                m_NameToHandleMap[name] = handle;
                StartAsyncMeshLoading(handle);
            }
            return handle;
        }

        VA_ENGINE_ERROR("[MeshSystem] Failed to create mesh handle for '{}'.", name);
        return Resources::InvalidMeshHandle;
    }

    Resources::IMesh* MeshSystem::GetPointerFor(const Resources::MeshHandle handle) const
    {
        const auto* node = m_MeshStorage.Get(handle);
        if (!node)
        {
            VA_ENGINE_ERROR("[MeshSystem] Invalid mesh handle.");

            // Return error mesh for invalid handles
            if (const auto* errorNode = m_MeshStorage.Get(m_ErrorMeshHandle); errorNode)
            {
                return errorNode->meshPtr.get();
            }

            return nullptr;
        }

        // Return actual mesh if loaded
        if (node->meshPtr)
        {
            return node->meshPtr.get();
        }

        // Handle different status appropriately
        switch (node->state)
        {
            case Resources::MeshLoadingState::Failed:
                // Failed -> Return error mesh
                if (const auto* errorNode = m_MeshStorage.Get(m_ErrorMeshHandle); errorNode)
                {
                    return errorNode->meshPtr.get();
                }
                break;

            default:
                break;
        }

        return nullptr;
    }

    uint32_t MeshSystem::GetIndexCountFor(const Resources::MeshHandle handle) const
    {
        const auto* mesh = GetPointerFor(handle);
        return mesh ? mesh->GetIndicesCount() : 0;
    }

    uint32_t MeshSystem::GetSubMeshCountFor(const Resources::MeshHandle handle) const
    {
        const auto* mesh = GetPointerFor(handle);
        return mesh ? mesh->GetSubMeshCount() : 0;
    }

    const Resources::SubMeshDescriptor& MeshSystem::GetSubMesh(
        const Resources::MeshHandle handle,
        const uint32_t submeshIndex) const
    {
        const auto* mesh = GetPointerFor(handle);
        if (mesh && submeshIndex < mesh->GetSubMeshCount())
        {
            return mesh->GetSubMesh(submeshIndex);
        }

        // Return reference to error mesh submesh as fallback
        if (const auto* errorNode = m_MeshStorage.Get(m_ErrorMeshHandle); errorNode)
        {
            return errorNode->meshPtr->GetSubMesh(0);
        }

        // This should never happen if an error mesh exists
        static Resources::SubMeshDescriptor fallback{};
        return fallback;
    }

    MaterialHandle MeshSystem::GetSubMeshMaterial(
        const Resources::MeshHandle handle,
        const uint32_t submeshIndex) const
    {
        const auto* mesh = GetPointerFor(handle);
        if (mesh && submeshIndex < mesh->GetSubMeshCount())
        {
            return mesh->GetSubMesh(submeshIndex).material;
        }

        // return default material as fallback
        return g_MaterialSystem->GetHandleForDefaultMaterial();
    }

    void MeshSystem::AddSubMeshTo(
        const Resources::MeshHandle handle,
        const std::string& submeshName,
        const MaterialHandle material,
        const VAArray<Resources::MeshVertex>& vertices,
        const VAArray<uint32_t>& indices) const
    {
        const auto* node = m_MeshStorage.Get(handle);
        if (!node || !node->meshPtr)
        {
            VA_ENGINE_ERROR("[MeshSystem] Invalid mesh handle or mesh not loaded.");
            return;
        }

        auto* mesh = node->meshPtr.get();
        auto meshData = mesh->GetMeshData();

        const uint32_t vertexOffset = static_cast<uint32_t>(meshData->vertices.size());
        const uint32_t indexOffset = static_cast<uint32_t>(meshData->indices.size());

        // Add geometry to mesh data
        meshData->AddSubmesh(vertices, indices);

        // Create a new submesh descriptor
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

        VA_ENGINE_TRACE(
            "[MeshSystem] Add submesh '{}' to mesh '{}' with {} vertices and {} indices.",
            submeshName,
            mesh->m_Name,
            vertices.size(),
            indices.size());
    }

    void MeshSystem::RemoveSubMeshFrom(
        const Resources::MeshHandle handle,
        const uint32_t submeshIndex) const
    {
        const auto* node = m_MeshStorage.Get(handle);
        if (!node || !node->meshPtr)
        {
            VA_ENGINE_ERROR("[MeshSystem] Invalid mesh handle or mesh not loaded.");
            return;
        }

        auto* mesh = node->meshPtr.get();
        if (submeshIndex >= mesh->GetSubMeshCount())
        {
            VA_ENGINE_ERROR(
                "[MeshSystem] Submesh index {} out of bounds for mesh '{}'.",
                submeshIndex,
                node->name);
            return;
        }

        const auto& submesh = mesh->GetSubMesh(submeshIndex);
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
        for (uint32_t i = submeshIndex; i < mesh->GetSubMeshCount(); ++i)
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
        auto* mesh = GetPointerFor(handle);
        if (mesh)
        {
            mesh->UpdateSubmeshMaterial(submeshIndex, material);
        }
    }

    //=========================================================================
    // Async Loading Pipeline Implementation
    //=========================================================================

    void MeshSystem::StartAsyncMeshLoading(Resources::MeshHandle handle)
    {
        if (!Jobs::g_JobSystem)
        {
            VA_ENGINE_ERROR("[MeshSystem] Failed to start async mesh loading - no job system.");
            return;
        }

        auto* node = m_MeshStorage.Get(handle);
        if (!node)
        {
            VA_ENGINE_ERROR("[MeshSystem] Failed to start async mesh loading - invalid handle.");
            return;
        }

        // Create SyncPoint for the complete loading pipeline
        auto completionSP = Jobs::g_JobSystem->CreateSyncPoint(1, "MeshLoaded");
        node->loadingComplete = completionSP;
        node->state = Resources::MeshLoadingState::Loading;

        // Job 1 : Load from disk (any worker)
        auto diskJobSP = Jobs::g_JobSystem->CreateSyncPoint(1, "MeshDiskLoad");
        auto diskJob = CreateMeshLoadJob(node->name);
        Jobs::g_JobSystem->Submit(diskJob, diskJobSP, Jobs::JobPriority::Normal, "MeshDiskLoad");

        // Job 2: GPU upload (main thread)
        auto gpuJob = CreateMeshUploadJob(node->name, handle);
        Jobs::g_JobSystem->SubmitAfter(
            diskJobSP,
            gpuJob,
            completionSP,
            Jobs::JobPriority::Normal,
            "MeshGPUUpload",
            Jobs::MAIN_THREAD_ONLY);

        VA_ENGINE_TRACE("[MeshSystem] Started async mesh loading for '{}'.", node->name);
    }

    Resources::MeshHandle MeshSystem::CreateMeshNode(const std::string& name)
    {
        const auto handle = m_MeshStorage.Allocate();
        if (!handle.IsValid())
        {
            VA_ENGINE_ERROR("[MeshSystem] Failed to allocate mesh slot for '{}'.", name);
            return Resources::InvalidMeshHandle;
        }

        // Initialize the new mesh node
        auto* node = m_MeshStorage.Get(handle);
        node->name = name;
        node->state = Resources::MeshLoadingState::Unloaded;
        node->meshPtr = nullptr;
        node->loadingComplete = Jobs::InvalidSyncPointHandle;

        return handle;
    }

    Jobs::JobFunction MeshSystem::CreateMeshLoadJob(const std::string& meshName)
    {
        return [this, meshName]() -> Jobs::JobResult
        {
            try
            {
                // Load mesh data via ResourceSystem
                auto meshDefinition = g_ResourceSystem->LoadResource<
                    Resources::Loaders::MeshDataDefinition>(ResourceType::Mesh, meshName);

                if (!meshDefinition || meshDefinition->GetVertices().empty())
                {
                    return Jobs::JobResult::Failed("Failed to load mesh definition.");
                }

                // Store directly in thread-safe container for main thread pickup
                m_LoadingStorage.StoreCompletedLoad(meshName, meshDefinition);

                VA_ENGINE_TRACE("[MeshSystem] Completed mesh disk load for '{}'.", meshName);
                return Jobs::JobResult::Success();
            }
            catch (const std::exception& e)
            {
                VA_ENGINE_ERROR("[MeshSystem] Failed to load mesh '{}': {}", meshName, e.what());
                return Jobs::JobResult::Failed(e.what());
            }
        };
    }

    Jobs::JobFunction MeshSystem::CreateMeshUploadJob(
        const std::string& meshName,
        Resources::MeshHandle handle)
    {
        return [this, meshName, handle]() -> Jobs::JobResult
        {
            // Retrieve completed mesh definition
            auto meshDefinition = m_LoadingStorage.RetrieveCompletedLoad(meshName);
            if (!meshDefinition)
            {
                return Jobs::JobResult::Failed("Failed to retrieve completed mesh definition.");
            }

            auto* node = m_MeshStorage.Get(handle);
            if (!node)
            {
                return Jobs::JobResult::Failed("Failed to retrieve mesh node.");
            }

            try
            {
                // Create GPU mesh using existing infrastructure
                auto meshData = std::make_shared<Resources::MeshData>(
                    meshDefinition->GetVertices(),
                    meshDefinition->GetIndices());

                auto* meshPtr = CreateMesh(meshName, meshData, meshDefinition->GetSubmeshes());
                if (!meshPtr)
                {
                    node->state = Resources::MeshLoadingState::Failed;
                    return Jobs::JobResult::Failed("Failed to create GPU mesh.");
                }

                // Update node with loaded mesh
                node->meshPtr.reset(meshPtr);
                node->state = Resources::MeshLoadingState::Loaded;

                VA_ENGINE_TRACE("[MeshSystem] Completed mesh GPU upload for '{}'.", meshName);
                return Jobs::JobResult::Success();
            }
            catch (const std::exception& e)
            {
                VA_ENGINE_ERROR("[MeshSystem] Failed to upload mesh '{}': {}", meshName, e.what());
                return Jobs::JobResult::Failed(e.what());
            }
        };
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

    void MeshSystem::CreateErrorMesh()
    {
        // Create simple error cube
        m_ErrorMeshHandle = CreateCube("__ErrorMesh");

        // Mark as error mesh to distinguish from regular cubes
        if (auto* node = m_MeshStorage.Get(m_ErrorMeshHandle))
        {
            node->state = Resources::MeshLoadingState::Failed;
        }

        VA_ENGINE_TRACE("[MeshSystem] Created error mesh.");
    }

    //=========================================================================
    // Procedural Mesh Creation Helper
    //=========================================================================

    Resources::MeshHandle MeshSystem::CreateProceduralMesh(
        const std::string& name,
        const VAArray<Resources::MeshVertex>& vertices,
        const VAArray<uint32_t>& indices,
        const VAArray<Resources::SubMeshDescriptor>& submeshes)
    {
        // If no submeshes are provided, create a single submesh mesh
        VAArray<Resources::SubMeshDescriptor> finalSubmeshes = submeshes;
        if (finalSubmeshes.empty())
        {
            Resources::SubMeshDescriptor singleSubmesh{
                name,
                g_MaterialSystem->GetHandleForDefaultMaterial(),
                0,
                static_cast<uint32_t>(indices.size()),
                0,
                static_cast<uint32_t>(vertices.size())
            };

            finalSubmeshes.push_back(singleSubmesh);
        }

        // Validate all submeshes
        auto meshData = std::make_shared<Resources::MeshData>(vertices, indices);
        for (const auto& submesh : finalSubmeshes)
        {
            if (!submesh.IsValid(*meshData))
            {
                VA_ENGINE_ERROR(
                    "[MeshSystem] Submesh '{}‘ for mesh '{}' is invalid.",
                    submesh.name,
                    name);
                return Resources::InvalidMeshHandle;
            }
        }

        // Create the mesh
        auto* meshPtr = CreateMesh(name, meshData, finalSubmeshes);
        if (!meshPtr)
        {
            VA_ENGINE_ERROR("[MeshSystem] Failed to create mesh '{}'.", name);
            return Resources::InvalidMeshHandle;
        }

        // Allocate handle and store mesh
        const auto handle = m_MeshStorage.Allocate();
        if (!handle.IsValid())
        {
            VA_ENGINE_ERROR("[MeshSystem] Failed to allocate mesh slot for '{}'.", name);
            delete meshPtr;
            return Resources::InvalidMeshHandle;
        }

        auto* node = m_MeshStorage.Get(handle);
        node->name = name;
        node->state = Resources::MeshLoadingState::Loaded;
        node->meshPtr.reset(meshPtr);
        node->loadingComplete = Jobs::InvalidSyncPointHandle;

        // Add to cache
        m_NameToHandleMap[name] = handle;

        VA_ENGINE_TRACE(
            "[MeshSystem] Created procedural mesh '{}' with handle ({}, {})) and {} submeshes.",
            name,
            handle.GetIndex(),
            handle.GetGeneration(),
            finalSubmeshes.size());

        return handle;
    }

    //=========================================================================
    // Procedural Generators Implementation
    //=========================================================================

    Resources::MeshHandle MeshSystem::CreateSphere(
        const std::string& name,
        const float radius,
        const uint32_t latitudeBands,
        const uint32_t longitudeBands)
    {
        VAArray<Resources::MeshVertex> vertices;
        VAArray<uint32_t> indices;

        // Generate sphere vertices
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

        // Generate sphere indices
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

        // Generate indices for all faces
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

    //=========================================================================
    // Math Helpers Implementation
    //=========================================================================

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
} // namespace VoidArchitect
