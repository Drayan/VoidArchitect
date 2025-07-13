//
// Created by Michael Desmedt on 13/07/2025.
//
#include "SDLSurfaceFactory.hpp"

#include <VoidArchitect/Engine/Common/Logger.hpp>

#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan_core.h>

namespace VoidArchitect::Platform
{
    SDLSurfaceFactory::SDLSurfaceFactory(SDL_Window* window)
        : m_Window(window)
    {
        VA_ENGINE_TRACE(
            "[SDLSurfaceFactory] Created for window 0x{:x}",
            reinterpret_cast<uintptr_t>(window));
    }

    RHI::SurfaceHandle SDLSurfaceFactory::CreateSurface(
        Platform::RHI_API_TYPE apiType,
        const RHI::SurfaceCreateInfo& params)
    {
        if (!IsAPISupported(apiType))
        {
            VA_ENGINE_ERROR(
                "[SDLSurfaceFactory] API '{}' is not supported.",
                static_cast<int>(apiType));
        }

        switch (apiType)
        {
            case Platform::RHI_API_TYPE::Vulkan:
            {
                // Return deferred surface - need VkInstance for completion
                auto creationData = CreateVulkanSurfaceData(params);
                if (!creationData)
                {
                    VA_ENGINE_ERROR(
                        "[SDLSurfaceFactory] Failed to create deferred surface for Vulkan.");
                    return {};
                }

                VA_ENGINE_DEBUG("[SDLSurfaceFactory] Deferred Vulkan surface created.");
                return RHI::SurfaceHandle::CreateDeferred(creationData, apiType);
            }

            case RHI_API_TYPE::DirectX12:
            case RHI_API_TYPE::Metal:
            case RHI_API_TYPE::OpenGL: VA_ENGINE_ERROR(
                    "[SDLSurfaceFactory] API {} not implemented",
                    static_cast<int>(apiType));
                return {};

            default: VA_ENGINE_ERROR("[SDLSurfaceFactory] Unknown API.");
                return {};
        }
    }

    RHI::SurfaceCreationCallback SDLSurfaceFactory::GetCreationCallback(
        Platform::RHI_API_TYPE apiType)
    {
        switch (apiType)
        {
            case RHI_API_TYPE::Vulkan:
                return RHI::SurfaceCreationCallback{
                    .apiType = apiType,
                    .finalize = &SDLSurfaceFactory::FinalizeVulkanSurface
                };

            case RHI_API_TYPE::OpenGL:
            case RHI_API_TYPE::DirectX12:
            case RHI_API_TYPE::Metal: default:
                // These APIs don't need deferred creation
                return {};
        }
    }

    void SDLSurfaceFactory::DestroySurface(RHI::SurfaceHandle& handle)
    {
        if (!handle.IsReady() && !handle.IsDeferred())
        {
            return;
        }

        VA_ENGINE_DEBUG(
            "[SDLSurfaceFactory] Destroying surface for API {}",
            static_cast<int>(handle.GetApiType()));

        switch (handle.GetApiType())
        {
            case RHI_API_TYPE::Vulkan:
                if (handle.IsReady())
                {
                    // Note: VkSurfaceKHR destruction requires VkInstance,
                    // so RHI must handle this before calling DestroySurface
                    VA_ENGINE_WARN(
                        "[SDLSurfaceFactory] Vulkan surface should be destroyed by RHI with VkInstance");
                }
                break;

            case RHI_API_TYPE::OpenGL:
                if (handle.IsReady())
                {
                    DestroyOpenGLContext(handle.GetNativeHandle());
                }
                break;

            default: VA_ENGINE_WARN("[SDLSurfaceFactory] Unknown surface type for destruction");
                break;
        }

        handle.Reset();
    }

    bool SDLSurfaceFactory::IsAPISupported(RHI_API_TYPE apiType) const
    {
        return CheckSDLSupport(apiType);
    }

    void* SDLSurfaceFactory::GetPlatformInfo(RHI_API_TYPE apiType) const
    {
        return m_Window;
    }

    VAArray<const char*> SDLSurfaceFactory::GetRequiredVulkanExtensions() const
    {
        unsigned int extensionCount = 0;
        auto extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);

        if (!extensions || extensionCount == 0)
        {
            VA_ENGINE_ERROR("[SDLSurfaceFactory] Failed to get Vulkan extensions.");
            return {};
        }

        VAArray<const char*> result;
        result.reserve(extensionCount);
        for (unsigned int i = 0; i < extensionCount; ++i)
        {
            result.push_back(extensions[i]);
        }

        VA_ENGINE_DEBUG("[SDLSurfaceFactory] Required Vulkan extensions: {}", result.size());
        for (const auto& ext : result)
        {
            VA_ENGINE_DEBUG("\t{}", ext);
        }

        return result;
    }

    void* SDLSurfaceFactory::CreateVulkanSurfaceData(const RHI::SurfaceCreateInfo& params)
    {
        // Store SDL window pointer as creation data
        // VulkanDevice will use this with its VkInstance later
        return static_cast<void*>(m_Window);
    }

    bool SDLSurfaceFactory::FinalizeVulkanSurface(
        void* vkInstance,
        void* windowPtr,
        void** outSurface)
    {
        auto* sdlWindow = static_cast<SDL_Window*>(windowPtr);
        auto instance = static_cast<VkInstance>(vkInstance);

        VkSurfaceKHR surface = VK_NULL_HANDLE;
        if (!SDL_Vulkan_CreateSurface(sdlWindow, instance, nullptr, &surface))
        {
            VA_ENGINE_ERROR(
                "[SDLSurfaceFactory] SDL_Vulkan_CreateSurface failed: {}",
                SDL_GetError());
            return false;
        }

        *outSurface = static_cast<void*>(surface);
        VA_ENGINE_DEBUG(
            "[SDLSurfaceFactory] Finalized Vulkan surface 0x{:x}",
            reinterpret_cast<uintptr_t>(surface));
        return true;
    }

    void* SDLSurfaceFactory::CreateOpenGLContext(const RHI::SurfaceCreateInfo& params)
    {
        // Configure OpenGL attributes based on params
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

        if (params.multisampleCount > 1)
        {
            SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
            SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, params.multisampleCount);
        }

        SDL_GLContext context = SDL_GL_CreateContext(m_Window);
        if (!context)
        {
            VA_ENGINE_ERROR(
                "[SDLSurfaceFactory] Failed to create OpenGL context: {}",
                SDL_GetError());
            return nullptr;
        }

        // Configure VSync
        if (SDL_GL_SetSwapInterval(params.enableVSync ? 1 : 0) != 0)
        {
            VA_ENGINE_WARN("[SDLSurfaceFactory] Failed to set VSync: {}", SDL_GetError());
        }

        return context;
    }

    void SDLSurfaceFactory::DestroyVulkanSurface(void* vkSurface, void* vkInstance)
    {
        if (vkSurface && vkInstance)
        {
            vkDestroySurfaceKHR(
                static_cast<VkInstance>(vkInstance),
                static_cast<VkSurfaceKHR>(vkSurface),
                nullptr);
        }
    }

    void SDLSurfaceFactory::DestroyOpenGLContext(void* context)
    {
        if (context)
        {
            SDL_GL_DestroyContext(static_cast<SDL_GLContext>(context));
        }
    }

    bool SDLSurfaceFactory::CheckSDLSupport(RHI_API_TYPE apiType) const
    {
        switch (apiType)
        {
            case RHI_API_TYPE::Vulkan:
                return true;

            case RHI_API_TYPE::OpenGL:
                return true; // SDL always supports OpenGL

            case RHI_API_TYPE::DirectX12:
            case RHI_API_TYPE::Metal:
                return false; // Not implemented via SDL

            default:
                return false;
        }
    }
}
