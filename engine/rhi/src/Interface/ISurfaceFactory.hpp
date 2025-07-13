//
// Created by Michael Desmedt on 13/07/2025.
//
#pragma once

#include "../Resources/SurfaceHandle.hpp"

namespace VoidArchitect::RHI
{
    /// @brief Surface creation parameters
    ///
    /// API-agnostic parameters for surface creation. Factories translate
    /// these to platform-specific configurations.
    struct SurfaceCreateInfo
    {
        /// @brief Preferred surface format (0 = default)
        uint32_t preferredFormat = 0;

        /// @brief Enable surface transparency support
        bool enableTransparency = false;

        /// @brief Enable high DPI scaling
        bool enableHighDPI = true;

        /// @brief Preferred swap chain buffer count
        uint32_t backBufferCount = 2;

        /// @brief Enable vertical synchronization
        bool enableVSync = true;

        /// @brief Anti-aliasing sample count
        uint32_t multisampleCount = 1;
    };

    /// @brief Two-phase surface creation callback
    ///
    /// Enables factories to complete surface creation when RHI context
    /// becomes available. Used for APIs requiring instance/device handles.
    struct SurfaceCreationCallback
    {
        /// @brief Function to finalize surface creation
        /// @param context API-specific context (VkInstance, ID3D12Device, etc.)
        /// @param creationData Platform creation data from SurfaceHandle
        /// @param outNativeHandle Receives created native surface
        /// @return true if surface creation succeeded
        using FinalizeFn = std::function<bool(
            void* context,
            void* creationData,
            void** outNativeHandle)>;

        /// @brief Graphics API this callback applies to
        Platform::RHI_API_TYPE apiType;

        /// @brief Finalization function
        FinalizeFn finalize;
    };

    // @brief Abstract factory for creating platform-agnostic rendering surfaces
    ///
    /// Creates native surfaces for graphics APIs without exposing platform details.
    /// Supports immediate creation (OpenGL) and deferred creation (Vulkan, DirectX).
    class ISurfaceFactory
    {
    public:
        // @brief Virtual destructor for proper cleanup
        virtual ~ISurfaceFactory() = default;

        /// @brief Create surface for specified graphics API
        /// @param apiType Graphics API to create surface for
        /// @param params Creation parameters (optional)
        /// @return Surface handle (may be deferred or finalized)
        ///
        /// Returns immediate surface for simple APIs (OpenGL) or deferred
        /// surface for complex APIs requiring additional context (Vulkan).
        virtual SurfaceHandle CreateSurface(
            Platform::RHI_API_TYPE apiType,
            const SurfaceCreateInfo& params = {}) = 0;

        /// @brief Get callback for completing deferred surface creation
        /// @param apiType Graphics API to get callback for
        /// @return Callback function or empty if API doesn't need callbacks
        ///
        /// RHI uses this callback to finalize surface creation when
        /// API context (VkInstance, device) becomes available.
        virtual SurfaceCreationCallback GetCreationCallback(Platform::RHI_API_TYPE apiType) = 0;

        /// @brief Destroy surface and release native resources
        /// @param handle Surface to destroy (invalidated after call)
        ///
        /// Must be called for all surfaces created by this factory.
        /// Handle becomes invalid after destruction.
        virtual void DestroySurface(SurfaceHandle& handle) = 0;

        /// @brief Check API support on current platform
        /// @param apiType Graphics API to check
        /// @return true if API surface creation is supported
        [[nodiscard]] virtual bool IsAPISupported(Platform::RHI_API_TYPE apiType) const = 0;

        /// @brief Get platform-specific surface information
        /// @param apiType Graphics API to query
        /// @return Platform-specific data or nullptr
        ///
        /// Returns implementation-specific metadata. Content varies by platform.
        [[nodiscard]] virtual void* GetPlatformInfo(Platform::RHI_API_TYPE apiType) const = 0;

        /// @brief Get required Vulkan instance extensions for this factory
        /// @return List of required extension names
        ///
        /// Returns platform-specific Vulkan extensions needed by this factory.
        [[nodiscard]] virtual VAArray<const char*> GetRequiredVulkanExtensions() const = 0;
    };
}
