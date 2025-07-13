//
// Created by Michael Desmedt on 01/06/2025.
//
#pragma once
#include <VoidArchitect/Engine/Common/Collections/FixedStorage.hpp>
#include <VoidArchitect/Engine/Common/Systems/Jobs/JobTypes.hpp>
#include <VoidArchitect/Engine/RHI/Resources/Mesh.hpp>
#include <VoidArchitect/Engine/RHI/Resources/MeshData.hpp>
#include <VoidArchitect/Engine/RHI/Resources/SubMesh.hpp>

namespace VoidArchitect
{
    namespace Resources
    {
        struct MeshVertex;

        namespace Loaders
        {
            class MeshDataDefinition;
        }
    }

    /// @brief Internal node tracking mesh state and async operations
    ///
    /// Each mesh handle corresponds to one MeshNode that tracks the
    /// current loading state, the actual mesh resource, and any ongoing
    /// async operations.
    struct MeshNode
    {
        std::string name; ///< Mesh identifier/filename
        Resources::MeshLoadingState state = Resources::MeshLoadingState::Unloaded;
        ///< Current loading state
        std::unique_ptr<Resources::IMesh> meshPtr = nullptr; ///< Actual mesh resource (when loaded)
        Jobs::SyncPointHandle loadingComplete = Jobs::InvalidSyncPointHandle;
        ///< Sync point for async operations

        MeshNode() = default;
        MeshNode(MeshNode&& other) noexcept = default;
        MeshNode& operator=(MeshNode&& other) noexcept = default;

        // Disable copy operations
        MeshNode(const MeshNode&) = delete;
        MeshNode& operator=(const MeshNode&) = delete;
    };

    /// @brief Thread-safe storage for completed mesh data from background job
    ///
    /// Provides a communication mechanism between background loading jobs
    /// and the main thread for completed mesh data. Uses mutex protection
    /// for simplicity while maintaining good performance for typical usage patterns.
    class MeshLoadingStorage
    {
    public:
        /// @brief Store completed mesh definition from a background job
        /// @param name Mesh name / identifier
        /// @param definition Shared pointer to loaded mesh definition (ownership shared)
        ///
        /// This method is called by background loading jobs when mesh data
        /// has been successfully loaded from disk. Thread-safe for concurrent access.
        void StoreCompletedLoad(
            const std::string& name,
            std::shared_ptr<Resources::Loaders::MeshDataDefinition> definition);

        /// @brief Retrieve and remove completed mesh definition
        /// @param name Name of the mesh to retrieve
        /// @return Shared pointer to mesh definition, or nullptr if not found
        ///
        /// This method is called from the main thread during GetHandleFor()
        /// to check if async loading has completed. Removes the data from storage
        /// to transfer ownership to the caller.
        std::shared_ptr<Resources::Loaders::MeshDataDefinition> RetrieveCompletedLoad(
            const std::string& name);

    private:
        mutable std::mutex m_Mutex; ///< Thread synchronization
        std::unordered_map<std::string, std::shared_ptr<Resources::Loaders::MeshDataDefinition>>
        m_CompletedLoads; ///< Completed mesh definitions
    };

    class MeshSystem
    {
    public:
        MeshSystem();
        ~MeshSystem() = default;

        MeshSystem(const MeshSystem&) = delete;
        MeshSystem& operator=(const MeshSystem&) = delete;

        /// @brief Get mesh handle for a given name, loading async if needed
        /// @param name Mesh name/identifier
        /// @param vertices Optional vertex data for procedural meshes
        /// @param indices Optional index data for procedural meshes
        /// @param submeshes Optional submesh descriptors for procedural meshes
        /// @return Handle to mesh resource (may be loading initially for file-based meshes)
        ///
        /// This is the primary entry point for mesh requests. If the mesh
        /// is not loaded, it will be requested asynchronously and a handle returned
        /// immediately.
        Resources::MeshHandle GetHandleFor(
            const std::string& name,
            const VAArray<Resources::MeshVertex>& vertices = {},
            const VAArray<uint32_t>& indices = {},
            const VAArray<Resources::SubMeshDescriptor>& submeshes = {});

        /// @brief Get pointer to mesh resource for rendering operations
        /// @param handle Handle to mesh resource
        /// @return Pointer to mesh resource, error mesh for failed loads, or nullptr during loading
        ///
        /// Returns the actual mesh if loaded, error mesh if loading failed,
        /// or nullptr if still loading.
        [[nodiscard]] Resources::IMesh* GetPointerFor(Resources::MeshHandle handle) const;

        /// @brief Add submesh to the existing mesh
        /// @param handle Handle to target mesh
        /// @param submeshName Name for the new submesh
        /// @param material Material handle for the submesh
        /// @param vertices Vertex data for the submesh
        /// @param indices Index data for the submesh
        void AddSubMeshTo(
            Resources::MeshHandle handle,
            const std::string& submeshName,
            MaterialHandle material,
            const VAArray<Resources::MeshVertex>& vertices,
            const VAArray<uint32_t>& indices) const;

        /// @brief Remove submesh from the existing mesh
        /// @param handle Handle to target mesh
        /// @param submeshIndex Index of submesh to remove
        void RemoveSubMeshFrom(Resources::MeshHandle handle, uint32_t submeshIndex) const;

        /// @brief Update material for a specific submesh
        /// @param handle Handle to target mesh
        /// @param submeshIndex Index of submesh to update
        /// @param material New material handle
        void UpdateSubMeshMaterial(
            Resources::MeshHandle handle,
            uint32_t submeshIndex,
            MaterialHandle material) const;

        /// @brief Get total index count for a mesh
        /// @param handle Handle to mesh resource
        /// @return Total number of indices in the mesh
        [[nodiscard]] uint32_t GetIndexCountFor(Resources::MeshHandle handle) const;

        /// @brief Get a number of submeshes in a mesh
        /// @param handle Handle to mesh resource
        /// @return Number of submeshes
        [[nodiscard]] uint32_t GetSubMeshCountFor(Resources::MeshHandle handle) const;

        /// @brief Get a submesh descriptor by index
        /// @param handle Handle to mesh resource
        /// @param submeshIndex Index of submesh to retrieve
        /// @return Reference to submesh descriptor
        [[nodiscard]] const Resources::SubMeshDescriptor& GetSubMesh(
            Resources::MeshHandle handle,
            uint32_t submeshIndex) const;

        /// @brief Get material handle for a specific submesh
        /// @param handle Handle to mesh resource
        /// @param submeshIndex Index of submesh
        /// @return Material handle for the submesh
        [[nodiscard]] MaterialHandle GetSubMeshMaterial(
            Resources::MeshHandle handle,
            uint32_t submeshIndex) const;

        //==========================================================================================
        // Basic shape procedural generators
        //==========================================================================================

        /// @brief Create procedural sphere mesh
        /// @param name Unique name for the sphere
        /// @param radius Sphere radius
        /// @param latitudeBands Number of latitude bands
        /// @param longitudeBands Number of longitude bands
        /// @return Handle to created sphere mesh
        Resources::MeshHandle CreateSphere(
            const std::string& name,
            float radius = 0.5f,
            uint32_t latitudeBands = 8,
            uint32_t longitudeBands = 8);

        /// @brief Create procedural cube mesh
        /// @param name Unique name for the cube
        /// @param material Material name to use
        /// @param size Cube size
        /// @return Handle to created cube mesh
        Resources::MeshHandle CreateCube(
            const std::string& name,
            const std::string& material = "DefaultMaterial",
            float size = 1.0f);

        /// @brief Create procedural quad mesh
        /// @param name Unique name for the quad
        /// @param width Quad width
        /// @param height Quad height
        /// @return Handle to created quad mesh
        Resources::MeshHandle CreateQuad(
            const std::string& name,
            float width = 1.0f,
            float height = 1.0f);

        /// @brief Create procedural plane mesh
        /// @param name Unique name for the plane
        /// @param width Plane width
        /// @param height Plane height
        /// @param normal Plane normal direction
        /// @param widthSegments Number of width segments
        /// @param heightSegments Number of height segments
        /// @return Handle to created plane mesh
        Resources::MeshHandle CreatePlane(
            const std::string& name,
            float width = 1.0f,
            float height = 1.0f,
            const Math::Vec3& normal = Math::Vec3::Up(),
            uint32_t widthSegments = 1,
            uint32_t heightSegments = 1);

        //==========================================================================================
        // Math helpers specialized for meshes
        //==========================================================================================

        /// @brief Generate normals for mesh vertices
        /// @param vertices Array of vertices to update
        /// @param indices Index array for face calculations
        static void GenerateNormals(
            VAArray<Resources::MeshVertex>& vertices,
            const VAArray<uint32_t>& indices);

        /// @brief Generate tangents for mesh vertices
        /// @param vertices Array of vertices to update
        /// @param indices Index array for face calculations
        static void GenerateTangents(
            VAArray<Resources::MeshVertex>& vertices,
            const VAArray<uint32_t>& indices);

    private:
        //=====================================================================
        // Async Loading Pipeline
        //=====================================================================

        /// @brief Start asynchronous loading for a mesh
        /// @param handle Handle of the mesh node to start loading for
        ///
        /// Initiates the async loadgin pipeline: disk I/O job followed by
        /// GPU upload jobs. Updates the mesh node state to Loading.
        void StartAsyncMeshLoading(Resources::MeshHandle handle);

        /// @brief Create a new mesh node and allocate a handle
        /// @param name Mesh name / identifier
        /// @return Handle to the newly created mesh node
        Resources::MeshHandle CreateMeshNode(const std::string& name);

        /// @brief Create a job function for disk-based mesh loading
        /// @param meshName Name of mesh to load
        /// @return Job function that performs disk I/O
        Jobs::JobFunction CreateMeshLoadJob(const std::string& meshName);

        /// @brief Create a job function for GPU mesh upload
        /// @param meshName Name of mesh to upload
        /// @param handle Handle to mesh node
        /// @return Job function that performs GPU upload (main thread only)
        Jobs::JobFunction CreateMeshUploadJob(
            const std::string& meshName,
            Resources::MeshHandle handle);

        /// @brief Create a mesh resource from data and submeshes
        /// @param name Mesh name
        /// @param data Shared pointer to mesh data
        /// @param submeshes Array of submesh descriptors
        /// @return Pointer to created mesh resource, or nullptr on failure
        static Resources::IMesh* CreateMesh(
            const std::string& name,
            std::shared_ptr<Resources::MeshData> data,
            const VAArray<Resources::SubMeshDescriptor>& submeshes);

        /// @brief Generate default error mesh for failed loads
        ///
        /// Creates a simple cube with error material to indicate loading failures.
        /// Called during system initialization.
        void CreateErrorMesh();

        /// @brief Create procedural mesh from provided geometry data
        /// @param name Mesh name / identifier
        /// @param vertices Vertex data array
        /// @param indices Index data array
        /// @param submeshes Submesh descriptor array
        /// @return Handle to created procedural mesh
        ///
        /// Helper method for handling procedural mesh creation with immediate
        /// GPU upload and proper handle allocation.
        Resources::MeshHandle CreateProceduralMesh(
            const std::string& name,
            const VAArray<Resources::MeshVertex>& vertices,
            const VAArray<uint32_t>& indices,
            const VAArray<Resources::SubMeshDescriptor>& submeshes);

        //=====================================================================
        // Storage and State Management
        //=====================================================================

        /// @brief Maximum number of meshes that can be loaded simultaneously
        static constexpr size_t MAX_MESHES = 1024;

        /// @brief Main mesh storage using handle-based system
        ///
        /// Uses FixedStorage for automatic generation management and ABA prevention.
        /// Each MeshNode is accessed via its MeshHandle which contains both
        /// the index and generation for safe access.
        FixedStorage<MeshNode, MAX_MESHES> m_MeshStorage;

        /// @brief Shared storage for async loading communication between background jobs and main thread
        MeshLoadingStorage m_LoadingStorage;

        /// @brief Cache mapping mesh names to their handles
        VAHashMap<std::string, Resources::MeshHandle> m_NameToHandleMap;

        /// @brief Handle to error mesh for failed loads
        ///
        /// Simple cube mesh with error material used as fallback when
        /// mesh loading fails. Created during system initialization.
        Resources::MeshHandle m_ErrorMeshHandle;
    };

    inline std::unique_ptr<MeshSystem> g_MeshSystem;
} // namespace VoidArchitect
