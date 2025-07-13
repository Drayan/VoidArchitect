//
// Created by Michael Desmedt on 13/07/2025.
//
#pragma once

#include <VoidArchitect/Engine/RHI/Interface/ISurfaceFactory.hpp>

#include <SDL3/SDL.h>

namespace VoidArchitect::Platform
{
    class SDLSurfaceFactory final : public RHI::ISurfaceFactory
    {
    public:
        /// @brief Create factory for SDL window
        /// @param window SDL window (must remain valid during factory lifetime)
        explicit SDLSurfaceFactory(SDL_Window* window);
        ~SDLSurfaceFactory() override = default;

        // === ISurfaceFactory implementation ===
        RHI::SurfaceHandle CreateSurface(
            Platform::RHI_API_TYPE apiType,
            const RHI::SurfaceCreateInfo& params) override;
        RHI::SurfaceCreationCallback GetCreationCallback(Platform::RHI_API_TYPE apiType) override;
        void DestroySurface(RHI::SurfaceHandle& handle) override;
        [[nodiscard]] bool IsAPISupported(Platform::RHI_API_TYPE apiType) const override;
        [[nodiscard]] void* GetPlatformInfo(Platform::RHI_API_TYPE apiType) const override;
        [[nodiscard]] VAArray<const char*> GetRequiredVulkanExtensions() const override;

    private:
        /// @brief SDL window for surface creation
        SDL_Window* m_Window;

        /// @brief Create deferred Vulkan surface data
        /// @param params Surface parameters
        /// @return SDL window pointer for deferred creation
        void* CreateVulkanSurfaceData(const RHI::SurfaceCreateInfo& params);

        /// @brief Finalize Vulkan surface creation
        /// @param vkInstance Vulkan instance handle
        /// @param windowPtr SDL window pointer
        /// @param outSurface Receives created VkSurfaceKHR
        /// @return true if creation succeeded
        static bool FinalizeVulkanSurface(void* vkInstance, void* windowPtr, void** outSurface);

        /// @brief Create immediate OpenGL context
        /// @param params Surface parameters
        /// @return OpenGL context or nullptr
        void* CreateOpenGLContext(const RHI::SurfaceCreateInfo& params);

        /// @brief Destroy Vulkan surface
        /// @param vkSurface VkSurfaceKHR to destroy
        /// @param vkInstance VkInstance for destruction
        void DestroyVulkanSurface(void* vkSurface, void* vkInstance);

        /// @brief Destroy OpenGL context
        /// @param context SDL_GLContext to destroy
        void DestroyOpenGLContext(void* context);

        /// @brief Check SDL compile-time API support
        /// @param apiType API to check
        /// @return true if SDL was compiled with API support
        bool CheckSDLSupport(RHI_API_TYPE apiType) const;
    };
}
