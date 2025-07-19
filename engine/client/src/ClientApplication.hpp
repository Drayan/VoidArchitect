//
// Created by Michael Desmedt on 09/07/2025.
//
#pragma once
#include <VoidArchitect/Engine/Common/Application.hpp>
#include <VoidArchitect/Engine/Common/Systems/Events/InputEvents.hpp>
#include <VoidArchitect/Engine/Common/Systems/Events/WindowEvents.hpp>

/// @file ClientApplication.hpp
/// @brief Client application class for windowed applications with rendering capabilities
///
/// This file defines the ClientApplication class which extends the base Application
/// to provide windowing, rendering, input handling, and resource management capabilities.
/// It serves as the foundation for interactive client applications including games and editors.
///
/// **Key systems integrated:**
/// - Window management and platform integration
/// - Rendering system with RHI abstraction
/// - Resource management for assets (textures, models, shaders)
/// - Input handling (keyboard, mouse, gamepad)
/// - Event processing for window and input events

namespace VoidArchitect
{
    // Forward declarations
    class Window;

    /// @brief Client application implementation with window management and rendering
    ///
    /// `ClientApplication` extends the base `Application` class to provide windowing,
    /// rendering, input handling capabilities, and other services required for interactive client
    /// applications. It serves as the foundation for both game clients and editor.
    ///
    /// **Additional Features over Base Application:**
    /// - Window management and platform integration
    /// - Rendering system with RHI abstraction
    /// - Resource management for assets
    /// - Input handling (keyboard, mouse, gamepad)
    /// - Window event processing (resize, close, focus)
    /// - Debug rendering modes and development tools
    ///
    /// **Application hierarchy position:**
    /// ```
    /// Application (base)
    /// ├─ ClientApplication (this class)
    /// |   └─ EditorApplication
    /// └─ ServerApplication
    /// ```
    ///
    /// **Initialisation order:**
    /// 1. Window creation and platform integration
    /// 2. Resource management system
    /// 3. Rendering system with selected graphics API
    /// 4. Rendering subsystems (shaders, textures, materials, meshes)
    ///
    /// @see `Application` for base application functionality
    /// @see `EditorApplication` for content creation tools
    /// @see `ServerApplication` for headless server implementations
    class ClientApplication : public Application
    {
    public:
        /// @brief Construct a client application with rendering capabilities
        ///
        /// @warning Main thread only - must be called from the main thread
        ///
        /// Calls the base Application constructor which will trigger the
        /// initialisation chain: base systems, then InitializeSubsystems()
        /// for client specific setup (window, rendering, resources).
        ///
        /// **Initialisation sequence:**
        /// 1. Base Application constructor
        /// 2. InitializeSubsystems() for client systems
        /// 3. Window creation and platform integration
        /// 4. Rendering system initialisation
        /// 5. Resource system setup
        ///
        /// @throws std::runtime_error if window creation fails
        /// @throws std::runtime_error if rendering system initialisation fails
        /// @throws std::exception if resource system initialisation fails
        ClientApplication();

        /// @brief Virtual destructor ensuring proper clean-up
        ///
        /// @warning Main thread only - must be called from the main thread
        ///
        /// Handles clean-up of all client-specific systems including rendering,
        /// resources, and window management. Clean-up is performed in reverse
        /// order of initialisation to ensure proper dependency handling.
        ///
        /// **Clean-up order:**
        /// 1. Client-specific systems (rendering, resources, window)
        /// 2. Base class clean-up (job system, event subscriptions)
        ///
        /// @note All pending GPU operations are completed before exit
        virtual ~ClientApplication();

    protected:
        /// @brief Initialise client-specific subsystems (window, rendering, resources)
        ///
        /// @warning Main thread only - called during application initialisation
        ///
        /// Sets up the complete client application infrastructure including window
        /// management, rendering pipeline, resource systems. Initialisation
        /// follows strict dependency order to ensure proper system integration.
        ///
        /// **Subsystems initialised:**
        /// - Window: Platform-specific window creation and event handling
        /// - ResourceSystem: Asset loading, caching, and management
        /// - RenderSystem: Main rendering coordination and RHI integration
        /// - ShaderSystem: Shader compilation, loading, and management
        /// - TextureSystem: Texture loading, compression and binding
        /// - MaterialSystem: Material creation, templates, and binding
        /// - MeshSystem: Geometry loading, optimisation and management
        /// - RenderStateSystem: GPU state management and optimisation
        /// - RenderPassSystem: Render pass configuration and execution
        ///
        /// **Dependency order:**
        /// Window → ResourceSystem → RenderSystem → Rendering subsystems
        ///
        /// @throws std::runtime_error if window creation fails
        /// @throws std::runtime_error if RHI initialisation fails
        /// @throws std::exception if any subsystem fails to initialise
        ///
        /// @note Called automatically during construction via base class
        /// @note Failure in any subsystem will abort application startup
        void InitializeSubsystems() override;

        /// @brief Perform per-frame updates for client-specific systems
        /// @param deltaTime Time elapsed since the last frame in seconds
        ///
        /// @warning Main thread only - called from the main application loop
        ///
        /// Handles frame-specific updates that don't require fixed timestep
        /// simulation. This includes variable-timestep animations, input
        /// processing, and system maintenance that adapts to frame rate.
        ///
        /// **Systems updated:**
        /// - Input system state updates
        /// - Animation system progression
        /// - UI system updates
        /// - Asset streaming and loading
        /// - Performance monitoring
        ///
        /// @note Called once per frame from the main thread after fixed updates
        /// @note Frame time is variable and depends on rendering performance
        void OnUpdate(float deltaTime) override;

        /// @brief Perform rendering operations for the current frame
        /// @param deltaTime Time elapsed since the last frame in seconds
        ///
        /// @warning Main thread only - called from the main application loop
        ///
        /// Executes the complete rendering pipeline for the current frame including
        /// all GPU operations, command submission, and frame presentation. This is
        /// the primary client-specific application update that distinguishes it
        /// from server applications.
        ///
        /// **Rendering pipeline:**
        /// 1. Scene culling and frustum calculations
        /// 2. Render pass execution (shadows, geometry, post-processing)
        /// 3. GPU command buffer submission
        /// 4. Frame presentation and swap chain management
        /// 5. GPU synchronisation and timing
        ///
        /// @note Called once per frame after `OnUpdate()`
        void OnLogic(float deltaTime) override;

        /// @brief Handle window close events
        /// @param e Window close event containing closure context
        ///
        /// @warning Main thread only - called via immediate event processing
        ///
        /// Handles window close requests from the operating system. Performs
        /// client-specific cleanup tasks before delegating to the base class
        /// implementation for application termination.
        ///
        /// **Client-specific cleanup:**
        /// - Complete pending GPU operations
        /// - Save rendering state and preferences
        /// - Release graphics resources
        /// - Destroy rendering contexts
        ///
        /// @note Calls base class OnWindowClose after client cleanup
        void OnWindowClose(const Events::WindowCloseEvent& e);
        
        /// @brief Handle window resize events
        /// @param e Window resize event containing new dimensions
        ///
        /// @warning Main thread only - called via immediate event processing
        ///
        /// Handles window resize events by updating all rendering systems
        /// to accommodate the new window dimensions. Critical for maintaining
        /// proper rendering surface and aspect ratios.
        ///
        /// **Systems updated:**
        /// - Rendering surface recreation (swap chains, render targets)
        /// - Camera projection matrices and aspect ratios
        /// - Viewport and scissor rectangle updates
        /// - UI layout recalculations
        /// - Post-processing effect buffers
        ///
        /// @note Resize operations may cause brief rendering interruptions
        void OnWindowResize(const Events::WindowResizedEvent& e);
        
        /// @brief Handle keyboard key press events
        /// @param e Key press event containing key code and repeat information
        ///
        /// @warning Main thread only - called via deferred event processing
        ///
        /// Processes keyboard input for client-specific functionality including
        /// debug commands, camera controls, and development tools. Does not
        /// handle general input processing which is managed by dedicated systems.
        ///
        /// **Common key bindings:**
        /// - Debug rendering toggles (wireframe, normals, bounds)
        /// - Performance overlay controls
        /// - Camera mode switching
        /// - Screenshot and recording functions
        ///
        /// @note General game input should be handled by input system, not here
        void OnKeyPressed(const Events::KeyPressedEvent& e);

        /// @brief Main window instance managed by the client application
        ///
        /// The window serves as the primary interface between the application
        /// and the operating system's windowing system. It provides the rendering
        /// surface and generates input/window events for application processing.
        ///
        /// **Responsibilities:**
        /// - Platform-specific window creation and management
        /// - Rendering surface provision for graphics API
        /// - Input event generation and forwarding
        /// - Window state management (minimised, maximised, fullscreen)
        std::unique_ptr<Window> m_MainWindow;

        /// @brief RAII subscription for window close events
        ///
        /// Automatically subscribes to WindowCloseEvent during initialisation
        /// and unsubscribes during destruction to prevent dangling handlers.
        /// Routes close events to OnWindowClose() for client-specific handling.
        Events::EventSubscription m_WindowCloseSubscription;
        
        /// @brief RAII subscription for window resize events
        ///
        /// Automatically subscribes to WindowResizedEvent during initialisation
        /// and unsubscribes during destruction. Routes resize events to
        /// OnWindowResize() for rendering system updates.
        Events::EventSubscription m_WindowResizeSubscription;
        
        /// @brief RAII subscription for keyboard input events
        ///
        /// Automatically subscribes to KeyPressedEvent during initialisation
        /// for client-specific keyboard handling (debug commands, camera controls).
        /// General game input is handled separately by the input system.
        Events::EventSubscription m_KeyPressedSubscription;
    };
} // VoidArchitect
