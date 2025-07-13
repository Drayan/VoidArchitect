//
// Created by Michael Desmedt on 12/07/2025.
//
#include "RHISystem.hpp"

#include <ranges>
#include <VoidArchitect/Engine/Common/Logger.hpp>
#include <VoidArchitect/Engine/Common/Window.hpp>
#include <VoidArchitect/Engine/RHI/Interface/IRenderingHardware.hpp>

// Forward declaration for platform-specific availability detection
namespace VoidArchitect::RHI::Platform
{
    bool IsVulkanAvailable();
    bool IsDirectX12Available();
    bool IsOpenGLAvailable();
    bool IsMetalAvailable();
}

// Forward declaration for Vulkan factory (when Vulkan backend is available)
#if defined(VOID_ARCH_RHI_VULKAN_AVAILABLE)
namespace VoidArchitect::RHI::Vulkan
{
    extern std::unique_ptr<VoidArchitect::Platform::IRenderingHardware> CreateVulkanRHI(
        std::unique_ptr<Window>& window);
}
#endif

namespace VoidArchitect::RHI
{
    RHISystem::RHISystem()
    {
        VA_ENGINE_INFO("[RHISystem] Initializing RHI system ...");

        try
        {
            // Register built-in backend factories
            RegisterBuiltinBackends();

            // Detect platform capabilities and update availability
            DetectPlatformCapabilities();

            // Initialize platform-specific backend priorities
            InitializePlatformPriorities();

            // Validate that at least one backend is available
            auto hasAvailableBackend = false;
            for (const auto& [apiType, info] : m_BackendInfos)
            {
                if (info.isAvailable)
                {
                    hasAvailableBackend = true;
                    VA_ENGINE_DEBUG(
                        "[RHISystem] Backend '{}' ({}) is available.",
                        info.name,
                        info.isRecommended ? "recommended" : "supported");
                }
            }

            if (!hasAvailableBackend)
            {
                VA_ENGINE_CRITICAL("[RHISystem] No RHI backend is available on this platform.");
                throw std::runtime_error("No RHI backend is available on this platform.");
            }

            VA_ENGINE_INFO(
                "[RHISystem] RHI system initialized with {} backend(s) available.",
                m_BackendInfos.size());
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_CRITICAL("[RHISystem] RHI system initialization failed: {}", e.what());
            throw;
        }
    }

    RHISystem::~RHISystem()
    {
        VA_ENGINE_INFO("[RHISystem] Shutting down RHI system ...");

        // Clear all registered factories and backend informations
        m_BackendFactories.clear();
        m_BackendInfos.clear();
        m_BackendPriority.clear();

        VA_ENGINE_INFO("[RHISystem] RHI system shutdown.");
    }

    std::unique_ptr<VoidArchitect::Platform::IRenderingHardware> RHISystem::CreateRHI(
        VoidArchitect::Platform::RHI_API_TYPE apiType,
        std::unique_ptr<Window>& window)
    {
        ValidateBackendType(apiType);

        if (apiType == VoidArchitect::Platform::RHI_API_TYPE::None)
        {
            return nullptr;
        }

        // Check if the requested backend is available
        auto infoIt = m_BackendInfos.find(apiType);
        if (infoIt == m_BackendInfos.end() || !infoIt->second.isAvailable)
        {
            VA_ENGINE_CRITICAL("[RHISystem] Backend '{}' is not available.", infoIt->second.name);
            throw std::runtime_error("Backend is not available.");
        }

        // Retrieve the factory function for this backend
        auto factoryIt = m_BackendFactories.find(apiType);
        if (factoryIt == m_BackendFactories.end())
        {
            VA_ENGINE_CRITICAL("[RHISystem] Backend '{}' has no factory.", infoIt->second.name);
            throw std::runtime_error("Backend has no factory.");
        }

        VA_ENGINE_INFO("[RHISystem] Creating RHI '{}' ...", infoIt->second.name);

        try
        {
            // Create the RHI instance using the registered factory
            auto rhi = factoryIt->second(window);
            if (!rhi)
            {
                VA_ENGINE_CRITICAL("[RHISystem] Failed to create RHI instance.");
                throw std::runtime_error("Failed to create RHI instance.");
            }

            VA_ENGINE_INFO("[RHISystem] RHI '{}' created.", infoIt->second.name);
            return rhi;
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_CRITICAL(
                "[RHISystem] Failed to create RHI '{}': {}",
                infoIt->second.name,
                e.what());
            throw;
        }
    }

    std::unique_ptr<VoidArchitect::Platform::IRenderingHardware> RHISystem::CreateBestAvailableRHI(
        std::unique_ptr<Window>& window)
    {
        VA_ENGINE_INFO("[RHISystem] Selecting best available RHI ...");

        // Try backends in priority order
        for (auto apiType : m_BackendPriority)
        {
            auto infoIt = m_BackendInfos.find(apiType);
            if (infoIt != m_BackendInfos.end() && infoIt->second.isAvailable)
            {
                try
                {
                    VA_ENGINE_DEBUG("[RHISystem] Trying backend '{}' ...", infoIt->second.name);
                    auto rhi = CreateRHI(apiType, window);

                    VA_ENGINE_INFO("[RHISystem] Backend '{}' selected.", infoIt->second.name);

                    return rhi;
                }
                catch (const std::exception& e)
                {
                    VA_ENGINE_WARN(
                        "[RHISystem] Backend '{}' failed: {}",
                        infoIt->second.name,
                        e.what());
                }
            }
        }

        // If we reach here, no backends could be created
        VA_ENGINE_CRITICAL("[RHISystem] No RHI backend could be created.");
        throw std::runtime_error("No RHI backend could be created.");
    }

    VAArray<RHIBackendInfo> RHISystem::GetAvailableBackends() const
    {
        VAArray<RHIBackendInfo> availableBackends;
        availableBackends.reserve(m_BackendInfos.size());

        for (const auto& info : m_BackendInfos | std::views::values)
        {
            if (info.isAvailable)
            {
                availableBackends.push_back(info);
            }
        }

        // Sort by recommendation status and then by name for consistent ordering
        std::ranges::sort(
            availableBackends,
            [](const auto& a, const auto& b)
            {
                if (a.isRecommended != b.isRecommended) return a.isRecommended;
                return std::string(a.name) < std::string(b.name);
            });

        return availableBackends;
    }

    bool RHISystem::IsBackendAvailable(VoidArchitect::Platform::RHI_API_TYPE apiType) const
    {
        auto it = m_BackendInfos.find(apiType);
        return it != m_BackendInfos.end() && it->second.isAvailable;
    }

    const char* RHISystem::GetBackendName(VoidArchitect::Platform::RHI_API_TYPE apiType) const
    {
        switch (apiType)
        {
            case VoidArchitect::Platform::RHI_API_TYPE::Vulkan:
                return "Vulkan";
            case VoidArchitect::Platform::RHI_API_TYPE::DirectX12:
                return "DirectX 12";
            case VoidArchitect::Platform::RHI_API_TYPE::OpenGL:
                return "OpenGL";
            case VoidArchitect::Platform::RHI_API_TYPE::Metal:
                return "Metal";
            default:
                return "Unknown";
        }
    }

    void RHISystem::RegisterBackend(
        VoidArchitect::Platform::RHI_API_TYPE apiType,
        RHIFactory factory,
        const RHIBackendInfo& info)
    {
        if (apiType == VoidArchitect::Platform::RHI_API_TYPE::None)
        {
            throw std::invalid_argument("Invalid backend type.");
        }

        if (!factory)
        {
            throw std::invalid_argument("Invalid backend factory.");
        }

        // Check if backend is already registered
        if (m_BackendFactories.find(apiType) != m_BackendFactories.end())
        {
            VA_ENGINE_WARN(
                "[RHISystem] Backend '{}' is already registered, overriding.",
                info.name);
        }

        m_BackendFactories[apiType] = factory;
        m_BackendInfos[apiType] = info;

        VA_ENGINE_TRACE(
            "[RHISystem] Backend '{}' ({}) registered.",
            info.name,
            info.isAvailable ? "available" : "unavailable");
    }

    void RHISystem::RegisterBuiltinBackends()
    {
#if defined(VOID_ARCH_RHI_VULKAN_AVAILABLE)
        // Register Vulkan backend
        RegisterBackend(
            VoidArchitect::Platform::RHI_API_TYPE::Vulkan,
            [](
            std::unique_ptr<Window>& window) -> std::unique_ptr<
            VoidArchitect::Platform::IRenderingHardware>
            {
                return Vulkan::CreateVulkanRHI(window);
            },
            RHIBackendInfo{
                .apiType = VoidArchitect::Platform::RHI_API_TYPE::Vulkan,
                .name = "Vulkan",
                .version = "1.3",
                .isAvailable = false,
                .isRecommended = false,
                .description = "Modern low-level graphics API."
            });
#endif

        // TODO: Add other backend registrations when implemented
        //  - DirectX 12 (Windows)
        //  - OpenGL (Corss-platform)
        //  - Metal (macOs/iOS)
    }

    void RHISystem::DetectPlatformCapabilities()
    {
        if (m_BackendInfos.find(VoidArchitect::Platform::RHI_API_TYPE::Vulkan) != m_BackendInfos.
            end())
        {
            auto vulkanAvailable = Platform::IsVulkanAvailable();
            m_BackendInfos[VoidArchitect::Platform::RHI_API_TYPE::Vulkan].isAvailable =
                vulkanAvailable;

            VA_ENGINE_DEBUG(
                "[RHISystem] Vulkan backend availability: {}",
                vulkanAvailable ? "Yes" : "No");
        }

        // TODO: Add detection for other backends
        //      - DirectX 12 availability detection
        //      - OpenGL availability detection
        //      - Metal availability detection
    }

    void RHISystem::InitializePlatformPriorities()
    {
        m_BackendPriority.clear();

#ifdef VOID_ARCH_PLATFORM_WINDOWS
        // Windows priority : DirectX 12 > Vulkan > OpenGL
        m_BackendPriority = {
            VoidArchitect::Platform::RHI_API_TYPE::DirectX12,
            VoidArchitect::Platform::RHI_API_TYPE::Vulkan,
            VoidArchitect::Platform::RHI_API_TYPE::OpenGL
        };

        // Mark DX12 as recommended on Windows if available
        if (m_BackendInfos.find(VoidArchitect::Platform::RHI_API_TYPE::DirectX12) != m_BackendInfos.
            end() && m_BackendInfos[VoidArchitect::Platform::RHI_API_TYPE::DirectX12].isAvailable)
        {
            m_BackendInfos[VoidArchitect::Platform::RHI_API_TYPE::DirectX12].isRecommended = true;
        }
#elif defined(VOID_ARCH_PLATFORM_MACOS)
        // MacOs priority Metal > Vulkan (MoltenVK) > OpenGL
        m_BackendPriority = {
            VoidArchitect::Platform::RHI_API_TYPE::Metal,
            VoidArchitect::Platform::RHI_API_TYPE::Vulkan,
            VoidArchitect::Platform::RHI_API_TYPE::OpenGL
        };

        // Mark Metal as recommended on MacOs if available
        if (m_BackendInfos.find(VoidArchitect::Platform::RHI_API_TYPE::Metal) != m_BackendInfos.
            end() && m_BackendInfos[VoidArchitect::Platform::RHI_API_TYPE::Metal].isAvailable)
        {
            m_BackendInfos[VoidArchitect::Platform::RHI_API_TYPE::Metal].isRecommended = true;
        }
#elif defined(VOID_ARCH_PLATFORM_LINUX)
        // Linux priority : Vulkan > OpenGL
        m_BackendPriority = {
            VoidArchitect::Platform::RHI_API_TYPE::Vulkan,
            VoidArchitect::Platform::RHI_API_TYPE::OpenGL
        };

        // Mark Vulkan as recommended on Linux if available
        if (m_BackendInfos.find(VoidArchitect::Platform::RHI_API_TYPE::Vulkan) != m_BackendInfos.
            end() && m_BackendInfos[VoidArchitect::Platform::RHI_API_TYPE::Vulkan].isAvailable)
        {
            m_BackendInfos[VoidArchitect::Platform::RHI_API_TYPE::Vulkan].isRecommended = true;
        }
#endif
    }

    void RHISystem::ValidateBackendType(VoidArchitect::Platform::RHI_API_TYPE apiType) const
    {
        if (m_BackendFactories.find(apiType) == m_BackendFactories.end())
        {
            VA_ENGINE_ERROR("[RHISystem] Backend '{}' is not registered.", GetBackendName(apiType));
            throw std::invalid_argument("Backend is not registered.");
        }
    }

    namespace Platform
    {
        bool Platform::IsVulkanAvailable()
        {
            //TODO: Implement proper Vulkan availability detection
            //  - Check for Vulkan loader lib
            //  - Verify compatible driver are installed

#ifdef VOID_ARCH_RHI_VULKAN_AVAILABLE
            return true;
#else
            return false;
#endif
        }

        bool Platform::IsDirectX12Available()
        {
            // TODO: Implement DirectX 12 availability detection
            //  - Check for D3D12 runtime on Windows
            //  - Verify compatible graphics drivers
#ifdef VOID_ARCH_PLATFORM_WINDOWS
            return false; // No implementation yet.
#else
            return false;
#endif
        }

        bool Platform::IsOpenGLAvailable()
        {
            // TODO: Implement OpenGL availability detection
            return false; // Not implemented yet.
        }

        bool Platform::IsMetalAvailable()
        {
            // TODO: Implement Metal availability detection
#ifdef VOID_ARCH_PLATFORM_MACOS
            return false; // Not implemented yet.
#else
            return false;
#endif
        }
    }
}
