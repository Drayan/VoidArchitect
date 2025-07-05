//
// Created by Michael Desmedt on 25/05/2025.
//
#pragma once

#include "Jobs/SyncPoint.hpp"
#include "Resources/Texture.hpp"

namespace VoidArchitect
{
    /// @brief Loading state for asynchronous texture operations
    ///
    /// Tracks the current state of texture loading to enable non-blocking
    /// texture requests and proper synchronization with the job system.
    /// State transitions: Unloaded -> Loading -> Loaded/Failed
    enum class TextureLoadState : uint8_t
    {
        Unloaded, ///< Texture not yet requested or loading not started
        Loading, ///< Asynchronous loading job is in progress
        Loaded, ///< Texture successfully loaded and available for use
        Failed, ///< Loading failed, error texture is being used as fallback
    };

    /// @brief Container for loaded texture data during async pipeline
    ///
    /// Stores pixel data and metadata loaded from disk before GPU upload.
    /// Uses RAII and move semantics to avoid unnecessary copies of large
    /// texture data during the async loading process.
    struct TextureLoadingData
    {
        std::string name; ///< Texture name/identifier
        std::unique_ptr<uint8_t[]> data; ///< Raw pixel data (owned)
        uint32_t width; ///< Texture width in pixels
        uint32_t height; ///< Texture height in pixels
        uint8_t channels; ///< Number of channels per pixel (1-4)
        bool hasTransparency; ///< Whether texture contains alpha channel data

        /// @brief Default constructor creates empty loading data
        TextureLoadingData() = default;

        TextureLoadingData(TextureLoadingData&& other) noexcept = default;
        TextureLoadingData& operator=(TextureLoadingData&& other) noexcept = default;

        // Disable copy operations to prevent accidental copies of large data
        TextureLoadingData(const TextureLoadingData&) = delete;
        TextureLoadingData& operator=(const TextureLoadingData&) = delete;
    };

    /// @brief Thread-safe storage for completed texture data from a background job
    ///
    /// Provides a communcation mechanism between background loading jobs
    /// and the main thread for completed texture data. Uses mutex protection
    /// for simplicity while maintaining good performance for typical usage patterns.
    class TextureLoadingStorage
    {
    public:
        /// @brief Store completed texture data from background job
        /// @param data Unique pointer to loaded texture data (ownership transferred)
        ///
        /// This method is called by background loading jobs when texture data
        /// has been successfully loaded from disk. Thread-safe for concurrent access.
        void StoreCompletedLoad(std::unique_ptr<TextureLoadingData> data);

        /// @brief Retrieve and remove completed texture data
        /// @param name Name of the texture to retrieve
        /// @return Unique pointer to texture data, or nullptr if not found
        ///
        /// This method is called from the main thread during GetHandleFor()
        /// to check if async loading has completed. Removes the data from storage
        /// to transfer ownership to the caller.
        std::unique_ptr<TextureLoadingData> RetrieveCompletedLoad(const std::string& name);

    private:
        mutable std::mutex m_Mutex; ///< Thread synchronization
        std::unordered_map<std::string, std::unique_ptr<TextureLoadingData>> m_CompletedLoads;
        ///< Storage for completed loads
    };

    /// @brief Internal node tracking texture state and async operations
    ///
    /// Each texture handle corresponds to one TextureNode that tracks the
    /// current loading state, the actual texture resource, and any ongoing
    /// async operations.
    struct TextureNode
    {
        std::string name; ///< Texture identifier/filename
        TextureLoadState state = TextureLoadState::Unloaded; ///< Current loading state
        std::unique_ptr<Resources::ITexture> texturePtr = nullptr;
        ///< Actual texture resource (when loaded)
        Jobs::SyncPointHandle loadingComplete = Jobs::InvalidSyncPointHandle;
        ///< Sync point for async operations

        TextureNode() = default;
        TextureNode(TextureNode&& other) noexcept = default;
        TextureNode& operator=(TextureNode&& other) noexcept = default;

        // Disable copy operations
        TextureNode(const TextureNode&) = delete;
        TextureNode& operator=(const TextureNode&) = delete;
    };

    class TextureSystem
    {
    public:
        TextureSystem();
        ~TextureSystem();

        Resources::TextureHandle GetHandleFor(const std::string& name);

        Resources::ITexture* GetPointerFor(Resources::TextureHandle handle) const;

        Resources::TextureHandle GetDefaultDiffuseHandle() const { return m_DefaultDiffuseHandle; }
        Resources::TextureHandle GetDefaultNormalHandle() const { return m_DefaultNormalHandle; }

        Resources::TextureHandle GetDefaultSpecularHandle() const
        {
            return m_DefaultSpecularHandle;
        }

        Resources::TextureHandle GetErrorTextureHandle() const { return m_ErrorTextureHandle; }

        /// @brief Get access to the loading storage for job system integration
        /// @return Reference to the texture loading storage
        ///
        /// This method provides access to the shared storage used for communication
        /// between background loading jobs and the main thread. Used internally
        /// by the async loading system.
        TextureLoadingStorage& GetLoadingStorage() { return m_LoadingStorage; }

    private:
        /// @brief Create a 2D texture with specified parameters synchronously
        /// @param name Texture identifier
        /// @param width Texture width in pixels
        /// @param height Texture height in pixels
        /// @param channels Number of color channels
        /// @param hasTransparency Whether texture has alpha channel
        /// @param data Raw pixel data
        /// @return Handle to created texture, or invalid handle on failure
        ///
        /// This method is used internally for creating default texture during
        /// system initialization. Regular texture should use the async pipeline
        /// through GetHandleFor().
        Resources::TextureHandle CreateTexture2D(
            const std::string& name,
            uint32_t width,
            uint32_t height,
            uint8_t channels,
            bool hasTransparency,
            const VAArray<uint8_t>& data);

        /// @brief Generate default textures (diffuse, normal, specular, error)
        void GenerateDefaultTextures();

        /// @brief Get a free texture handle for new texture allocation
        /// @return Available texture handle
        uint32_t GetFreeTextureHandle();

        /// @brief Release a texture and make its handle available for reuse
        /// @param texture Texture to release (currently unused))
        void ReleaseTexture(const Resources::ITexture* texture);

        /// @brief Start asynchronous loading for a texture
        /// @param handle Handle of the texture node to start loading for
        ///
        /// Initiates the async loading pipeline: disk I/O job followed by
        /// GPU upload jobs. Updates the texture node state to Loading.
        void StartAsyncTextureLoading(Resources::TextureHandle handle);

        /// @brief Create a new texture node and allocate handle
        /// @param name Texture name/identifier
        /// @return Handle to the newly created texture node
        Resources::TextureHandle CreateTextureNode(const std::string& name);

        /// @brief Create a job function for disk-based texture loading
        /// @param textureName Name of texture to load
        /// @return Job function that performs disk I/O
        Jobs::JobFunction CreateTextureLoadJob(const std::string& textureName);

        /// @brief Create a job function for GPU texture upload
        /// @param textureName Name of texture to upload
        /// @param handle
        /// @return Job function that performs GPU upload (main thread only)
        Jobs::JobFunction CreateTextureUploadJob(
            const std::string& textureName,
            Resources::TextureHandle handle);

        // === Storage and State management ===

        std::queue<uint32_t> m_FreeTextureHandles; ///< Pool of available texture handles
        uint32_t m_NextTextureHandle = 0; ///< Next handle to allocate if pool empty

        VAArray<TextureNode> m_TextureNodes; ///< Node storage for texture state tracking
        TextureLoadingStorage m_LoadingStorage; ///< Shared storage for async loading communication.

        // === Default Texture Handles ===

        Resources::TextureHandle m_DefaultDiffuseHandle;
        Resources::TextureHandle m_DefaultNormalHandle;
        Resources::TextureHandle m_DefaultSpecularHandle;
        Resources::TextureHandle m_ErrorTextureHandle;
    };

    inline std::unique_ptr<TextureSystem> g_TextureSystem;
} // namespace VoidArchitect
