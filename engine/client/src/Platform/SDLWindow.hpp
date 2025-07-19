//
// Created by Michael Desmedt on 13/05/2025.
//
#pragma once
#include <VoidArchitect/Engine/Common/Window.hpp>

#include <SDL3/SDL.h>

/// @file SDLWindow.hpp
/// @brief SDL3-based window implementation for cross-platform windowing support
///
/// This file defines the SDLWindow class which provides a concrete implementation
/// of the Window interface using SDL3 as the underlying windowing system.
/// SDL3 provides cross-platform window management, input handling, and
/// graphics context creation capabilities.
///
/// **Platform support:**
/// - Windows (Win32)
/// - macOS (Cocoa)
/// - Linux (X11/Wayland)
/// - Additional platforms supported by SDL3
///
/// **Integration with graphics APIs:**
/// - Vulkan surface creation
/// - OpenGL context management
/// - Metal surface support (macOS)
/// - DirectX integration (Windows)

namespace VoidArchitect::Platform
{
    /// @brief SDL3-based window implementation for cross-platform windowing
    ///
    /// SDLWindow provides a concrete implementation of the Window interface using
    /// SDL3 as the underlying windowing system. It handles window creation,
    /// management, event processing, and graphics surface creation across
    /// multiple platforms.
    ///
    /// **Key features:**
    /// - Cross-platform window creation and management
    /// - Graphics API surface factory integration
    /// - VSync control and management
    /// - Event processing and forwarding to the engine's event system
    /// - High DPI display support
    /// - Multiple monitor configuration
    ///
    /// **SDL3 integration:**
    /// - Utilises SDL3's latest windowing API
    /// - Supports modern display features
    /// - Handles platform-specific window behaviours
    /// - Integrates with SDL3's event system
    ///
    /// **Usage example:**
    /// @code
    /// WindowProps props;
    /// props.Title = "VoidArchitect Engine";
    /// props.Width = 1920;
    /// props.Height = 1080;
    /// props.VSync = true;
    ///
    /// auto window = std::make_unique<SDLWindow>(props);
    /// auto surfaceFactory = window->CreateSurfaceFactory();
    /// @endcode
    ///
    /// @see Window for the base window interface
    /// @see WindowProps for window configuration options
    class SDLWindow : public Window
    {
    public:
        /// @brief Construct an SDL window with specified properties
        /// @param props Window configuration including size, title, and display options
        ///
        /// @warning Main thread only - must be called from the main thread
        ///
        /// Creates a new SDL window with the specified properties. This includes
        /// SDL subsystem initialisation, window creation, and initial setup.
        /// The window is ready for use immediately after construction.
        ///
        /// **Initialisation steps:**
        /// 1. SDL video subsystem initialisation
        /// 2. Window creation with specified properties
        /// 3. Graphics context preparation
        /// 4. Event system integration
        ///
        /// @throws std::runtime_error if SDL initialisation fails
        /// @throws std::runtime_error if window creation fails
        /// @throws std::exception if graphics context setup fails
        SDLWindow(const WindowProps& props);
        
        /// @brief Destroy the SDL window and cleanup resources
        ///
        /// @warning Main thread only - must be called from the main thread
        ///
        /// Performs cleanup of all SDL resources including window destruction
        /// and subsystem shutdown. Ensures proper resource deallocation and
        /// prevents memory leaks.
        ///
        /// **Cleanup order:**
        /// 1. Graphics context cleanup
        /// 2. Window destruction
        /// 3. SDL subsystem shutdown
        virtual ~SDLWindow();

        /// @brief Create a surface factory for the selected graphics API
        /// @return Unique pointer to surface factory instance
        ///
        /// @warning Main thread only - must be called from the main thread
        ///
        /// Creates a graphics API-specific surface factory that can create
        /// rendering surfaces compatible with this window. The factory type
        /// is determined by the current RHI configuration.
        ///
        /// **Supported graphics APIs:**
        /// - Vulkan: Creates VulkanSurfaceFactory
        /// - OpenGL: Creates OpenGLSurfaceFactory
        /// - Metal: Creates MetalSurfaceFactory (macOS)
        /// - DirectX: Creates DirectXSurfaceFactory (Windows)
        ///
        /// @throws std::runtime_error if graphics API is not supported
        /// @throws std::exception if surface factory creation fails
        std::unique_ptr<RHI::ISurfaceFactory> CreateSurfaceFactory() override;

        /// @brief Process SDL events and update window state
        ///
        /// @warning Main thread only - called from the main application loop
        ///
        /// Polls SDL events and processes window-related events including
        /// resize, close, focus changes, and input events. Events are
        /// translated to engine events and forwarded to the event system.
        ///
        /// **Events processed:**
        /// - Window resize, move, close, focus/unfocus
        /// - Keyboard and mouse input
        /// - Display configuration changes
        /// - System-level window events
        ///
        /// @note Called once per frame from the main application loop
        void OnUpdate() override;

        /// @brief Get the current window width in pixels
        /// @return Window width in pixels, or 0 if window is invalid
        ///
        /// @warning Thread-safe - can be called from any thread
        ///
        /// Retrieves the current window width in pixels using SDL's pixel-based
        /// size query. This accounts for high DPI displays and returns the actual
        /// drawable area size rather than logical window size.
        ///
        /// @note Returns 0 if the window has not been created or is invalid
        unsigned int GetWidth() const override
        {
            if (!m_Window) return 0;

            int width;
            SDL_GetWindowSizeInPixels(m_Window, &width, nullptr);
            return static_cast<unsigned int>(width);
        };

        /// @brief Get the current window height in pixels
        /// @return Window height in pixels, or 0 if window is invalid
        ///
        /// @warning Thread-safe - can be called from any thread
        ///
        /// Retrieves the current window height in pixels using SDL's pixel-based
        /// size query. This accounts for high DPI displays and returns the actual
        /// drawable area size rather than logical window size.
        ///
        /// @note Returns 0 if the window has not been created or is invalid
        unsigned int GetHeight() const override
        {
            if (!m_Window) return 0;

            int height;
            SDL_GetWindowSizeInPixels(m_Window, nullptr, &height);
            return static_cast<unsigned int>(height);
        }

        /// @brief Enable or disable vertical synchronisation
        /// @param enabled true to enable VSync, false to disable
        ///
        /// @warning Main thread only - must be called from the main thread
        ///
        /// Controls vertical synchronisation for the window's rendering surface.
        /// VSync synchronises frame presentation with the display's refresh rate
        /// to prevent screen tearing.
        ///
        /// **Effects of enabling VSync:**
        /// - Eliminates screen tearing
        /// - Limits frame rate to display refresh rate
        /// - May introduce input latency
        /// - Reduces GPU power consumption
        ///
        /// @note VSync setting affects the entire window surface
        void SetVSync(bool enabled) override { SDL_SetWindowSurfaceVSync(m_Window, enabled); }

        /// @brief Check if vertical synchronisation is enabled
        /// @return true if VSync is enabled, false otherwise
        ///
        /// @warning Thread-safe - can be called from any thread
        ///
        /// Queries the current VSync state of the window's rendering surface.
        /// This reflects the actual VSync setting rather than a cached value.
        ///
        /// @note May return false if VSync is not supported by the display
        bool IsVSync() const override
        {
            int enabled;
            SDL_GetWindowSurfaceVSync(m_Window, &enabled);
            return enabled > 0;
        }

        /// @brief Get the native SDL window handle
        /// @return Pointer to the underlying SDL_Window, or nullptr if invalid
        ///
        /// @warning Thread-safe - can be called from any thread
        ///
        /// Provides direct access to the underlying SDL_Window for advanced
        /// operations or integration with third-party libraries that require
        /// native window handles.
        ///
        /// **Use cases:**
        /// - Graphics API surface creation
        /// - Third-party library integration
        /// - Platform-specific window operations
        /// - Advanced SDL feature access
        ///
        /// @note Handle becomes invalid after window destruction
        SDL_Window* GetNativeWindow() const { return m_Window; }

    private:
        /// @brief Initialise SDL subsystems and create the window
        /// @param props Window configuration properties
        ///
        /// @warning Main thread only - called during construction
        ///
        /// Performs the actual SDL initialisation and window creation process.
        /// This includes SDL video subsystem setup, window creation with
        /// specified properties, and initial graphics context preparation.
        ///
        /// **Initialisation steps:**
        /// 1. Verify SDL video subsystem is available
        /// 2. Configure window creation flags based on properties
        /// 3. Create SDL window with specified dimensions and title
        /// 4. Set initial window state (VSync, fullscreen, etc.)
        /// 5. Prepare graphics context for API integration
        ///
        /// @throws std::runtime_error if SDL subsystem initialisation fails
        /// @throws std::runtime_error if window creation fails
        /// @throws std::exception if initial configuration fails
        virtual void Initialize(const WindowProps& props);
        
        /// @brief Shutdown SDL subsystems and cleanup window resources
        ///
        /// @warning Main thread only - called during destruction
        ///
        /// Performs cleanup of all SDL-related resources in the correct order
        /// to prevent resource leaks and ensure proper shutdown.
        ///
        /// **Shutdown steps:**
        /// 1. Destroy graphics contexts
        /// 2. Destroy SDL window
        /// 3. Cleanup SDL video subsystem
        /// 4. Release any cached resources
        ///
        /// @note Called automatically from destructor
        virtual void Shutdown();

    private:
        /// @brief Native SDL window handle
        ///
        /// Stores the underlying SDL_Window pointer that represents the actual
        /// platform window. This handle is used for all SDL window operations
        /// including size queries, surface creation, and event processing.
        ///
        /// **Lifecycle:**
        /// - Created during Initialize()
        /// - Used throughout window lifetime
        /// - Destroyed during Shutdown()
        /// - Set to nullptr after destruction
        ///
        /// **Thread safety:**
        /// Most SDL window operations are thread-safe for read access,
        /// but modifications should be performed on the main thread.
        SDL_Window* m_Window;
    };
} // namespace VoidArchitect::Platform
