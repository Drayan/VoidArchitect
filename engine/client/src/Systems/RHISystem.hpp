//
// Created by Michael Desmedt on 12/07/2025.
//
#pragma once

#include "../../../rhi/src/Interface/IRenderingHardware.hpp"

namespace VoidArchitect
{
    class Window;

    namespace RHI
    {
        /// @brief Information about an available RHI backend
        ///
        /// Provides metadata about RHI implementations that can be loaded
        /// by the RHISystem. This information helps applications choose
        /// the most appropriate backend based on platform capabilities
        /// and user preferences.
        struct RHIBackendInfo
        {
            /// @brief Type identifier for this RHI backend
            Platform::RHI_API_TYPE apiType;

            /// @brief Human-readable name for the API (e.g., "Vulkan", "DirectX 12")
            const char* name;

            /// @brief Backend version information (e.g., "1.3.0")
            const char* version;

            /// @brief Whether this backend is available on the current platform
            bool isAvailable;

            /// @brief Whether this backend is recommended for the current platform
            bool isRecommended;

            /// @brief Brief description of backend capabilities and limitations
            const char* description;
        };

        /// @brief Factory function signature for creating RHI implementations
        /// @param window Window to create the RHI context for
        /// @return Unique pointer to the created RHI implementation
        /// @throws std::runtime_error if RHI creation fails
        ///
        /// RHI backends must provide a factory function matching this signature
        /// to be compatible with the dynamic loading system. The factory function
        /// is responsible for creating and initializing the backend with the
        /// provided window context.
        using RHIFactory = std::function<std::unique_ptr<Platform::IRenderingHardware>(
            std::unique_ptr<Window>&)>;

        /// @brief Central management system for RHI backend loading and lifecycle
        ///
        /// RHISystem provides a unified interface for discovering, loading and managing
        /// different RHI implementations (Vulkan, DirectX 12, OpenGL, Metal). It handles
        /// dynamic backend selection, initialization, and proper cleanup while maintaining
        /// API-agnostic client code.
        ///
        /// **Usage:**
        /// @code
        /// // Automatic backend selection
        /// auto rhi = g_RHISystem->CreateBestAvailableRHI(window);
        ///
        /// // Explicit backend selection
        /// auto rhi = g_RHISystem->CreateRHI(RHI_API_TYPE::Vulkan, window);
        ///
        /// // Backend discovery
        /// auto backends = g_RHISystem->GetAvailableBackends();
        /// for(const auto& backend : backends)
        /// {
        ///     if(backend.isRecommended)
        ///     {
        ///         // Use this backend
        ///     }
        /// }
        /// @endcode
        class RHISystem
        {
        public:
            /// @brief Default constructor initialize the RHI system and discover available backends
            ///
            /// Performs a one-time initialization of the RHI system including backend
            /// discovery, availability detection, and factory registration.
            ///
            /// @throws std::runtime_error if no RHI backends are available
            /// @throws std::exception if critical system resources cannot be accessed
            RHISystem();

            /// @brief Shutdown the RHI system and clean-up resources
            ///
            /// Performs clean-up of the RHI system including active backend shutdown,
            /// factory deregistration, and resource clean-up.
            ~RHISystem();

            // Don't allow copy
            RHISystem(const RHISystem&) = delete;
            RHISystem& operator=(const RHISystem&) = delete;

            /// @brief Create an RHI interface using the specified backend type
            /// @param apiType Type of RHI backend to create
            /// @param window Window to create the RHI context for
            /// @return Unique pointer to the created RHI implementation
            ///
            /// Creates a new RHI instance using the specified backend type. The backend
            /// must be available on the current platform.
            ///
            /// @throws std::invalid_argument if apiType is None or invalid
            /// @throws std::runtime_error if the requested backend is not available
            /// @throws std::runtime_error if RHI creation fails.
            ///
            /// @note Window must remain valid for the lifetime of the RHI instance.
            std::unique_ptr<Platform::IRenderingHardware> CreateRHI(
                Platform::RHI_API_TYPE apiType,
                std::unique_ptr<Window>& window);

            /// @brief Create an RHI instance using the best available backend for the platform
            /// @param window Window to create the RHI context for
            /// @return Unique pointer to the created RHI implementation
            ///
            /// Automatically selects and creates the most appropriate RHI backend for
            /// the current platform based on availability, performance and feature set.
            /// This method provides a convenient way to get optimal performance
            /// without manual backend selection.
            ///
            /// @throws std::runtime_error if no backends are available on the platform.
            /// @throws std::runtime_error if RHI creation fails for all available backends.
            std::unique_ptr<Platform::IRenderingHardware> CreateBestAvailableRHI(
                std::unique_ptr<Window>& window);

            /// @brief Get information about all available RHI backends
            /// @return Vector of backend information structures.
            ///
            /// Returns comprehensive information about all RHI backends that have been
            /// discovered and registered with the system. This information can be used
            /// for user selection interfaces, debugging, or advanced configuration.
            VAArray<RHIBackendInfo> GetAvailableBackends() const;

            /// @brief Check if a specific RHI backend is available on the current platform
            /// @param apiType Type of RHI backend to check
            /// @return true if the backend is available, false otherwise.
            ///
            /// Provides a quick way to check backend availability without attempting
            /// to create an instance. Useful for conditional code paths and early
            /// validation of backend requirements.
            bool IsBackendAvailable(Platform::RHI_API_TYPE apiType) const;

            /// @brief Get the human-readable name for and RHI backend type
            /// @param apiType Type of RHI backend
            /// @return String name of the backend (e.g., "Vulkan", "DirectX 12")
            ///
            /// Provides consistent string representations of RHI backend types ofr
            /// logging, user interfaces, and debugging purposes.
            ///
            /// @note Returns "Unknown" for unrecognized backend types
            const char* GetBackendName(Platform::RHI_API_TYPE apiType) const;

            /// @brief Register a custom RHI backend factory function
            /// @param apiType Type identifier for the custom backend
            /// @param factory Factory function that creates instances of the backend
            /// @param info Information struture describing the backend capabilities
            ///
            /// Allows registration of custom or third-party RHI backends that can be
            /// loaded and managed by the RHISystem. This enables extensibility without
            /// modifying the core RHI system code.
            ///
            /// @throws std::invalid_argument if apiType is already registered.
            /// @throws std::invalid_argument if factory function is nullptr.
            void RegisterBackend(
                Platform::RHI_API_TYPE apiType,
                RHIFactory factory,
                const RHIBackendInfo& info);

        private:
            /// @brief Storage for registered backend factories
            VAHashMap<Platform::RHI_API_TYPE, RHIFactory> m_BackendFactories;

            /// @brief Storage for backend information and availability
            VAHashMap<Platform::RHI_API_TYPE, RHIBackendInfo> m_BackendInfos;

            /// @brief Platform-specific backend priority ordering
            VAArray<Platform::RHI_API_TYPE> m_BackendPriority;

            /// @brief Register built-in backend factories during initialization
            ///
            /// Registers the factory functions for all built-in RHI backends
            /// (Vulkan, DirectX12, OpenGL, Metal). Each factory is responsible
            /// for creating instances of its respective backend type.
            void RegisterBuiltinBackends();

            /// @brief Detect platform capabilities and update backend availability
            ///
            /// Performs runtime detection of platform capabilities including
            /// driver availability, hardware support, and runtime library presence.
            /// Updates the availability information for all registered backends.
            void DetectPlatformCapabilities();

            /// @brief Initialize platform-specific backend priority ordering
            ///
            /// Sets up the platform-specific ordering for automatic backend selection.
            /// Priority is based on performance characteristics, features completeness,
            /// and platform-specific optimizations.
            void InitializePlatformPriorities();

            /// @brief Validate that a backend type is valid and registeredd
            /// @param apiType Backend type to validate
            /// @throws std::invalid_argument if backend type is invalid or not registered.
            void ValidateBackendType(Platform::RHI_API_TYPE apiType) const;
        };

        inline std::unique_ptr<RHISystem> g_RHISystem = nullptr;
    }
}
