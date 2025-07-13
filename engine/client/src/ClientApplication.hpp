//
// Created by Michael Desmedt on 09/07/2025.
//
#pragma once
#include <VoidArchitect/Engine/Common/Application.hpp>
#include <VoidArchitect/Engine/Common/Events/Event.hpp>
#include <VoidArchitect/Engine/Common/Events/ApplicationEvent.hpp>
#include <VoidArchitect/Engine/Common/Events/KeyEvent.hpp>

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
    /// **Application Hierarchy Position:**
    /// ```
    /// Application (base)
    /// ├─ ClientApplication (this class)
    /// |   └─ EditorApplication
    /// └─ ServerApplication
    /// ```
    ///
    /// **Initialization Order:**
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
        /// @brief Constructs a client application with rendering capabilities
        ///
        /// Calls the base Application constructor which will trigger the
        /// initialization chain: base systems, then InitializeSubsystems()
        /// for client specific setup (window, rendering, resources).
        ///
        /// @throws std::runtime_error if window creation fails
        /// @throws std::runtime_error if rendering system initialization fails
        /// @throws std::exception if resource system initialization fails
        ClientApplication();

        /// @brief Virtual destructor ensuring proper clean-up
        ///
        /// Handles clean-up of all client-specific systems including rendering,
        /// resources, and window management. Clean-up is performed in reverse
        /// order of initialization to ensure proper dependency handling.
        ///
        /// **Clean-up order:**
        /// 1. Client-specific systems (rendering, resources, window)
        /// 2. Base class clean-up (job system, layers)
        ///
        /// @note All pending GPU operations are completed before exit
        virtual ~ClientApplication();

    protected:
        /// @brief Initialize client-specific subsystems (window, rendering, resources, ...)
        ///
        /// Sets up the complete client application infrastructure including window
        /// management, rendering pipeline, resource systems... Initialization
        /// follows struct dependency order to ensure proper system integration.
        ///
        /// **Subsystems initialized:**
        /// - Window: Platform-specific window creation and event handling
        /// - ResourceSystem: Asset loading, caching, and management
        /// - RenderSystem: Main rendering coordination and RHI integration
        /// - ShaderSystem: Shader compilation, loading, and managment
        /// - TextureSystem: Texture loading, compression and binding
        /// - MaterialSystem: Material creation, templates, and binding
        /// - MeshSystem: Geometry loading, optimization and management
        /// - RenderStateSystem: GPU state management and optimization
        /// - RenderPassSystem: Render pass configuration and execution
        ///
        /// @throws std::runtime_error if window creation fails
        /// @throws std::runtime_error if RHI initialization fails
        /// @throws std::exception if any subsystem fails to initialize
        ///
        /// @note Called automatically during construction via base class
        /// @note Failure in any subsystem will abort application startup
        void InitializeSubsystems() override;

        /// @brief Performs per-frame updates for client-specific systems
        /// @param deltaTime Time elapsed since the last frame in seconds
        ///
        /// Handles frame-specific updates that don't require fixed timesteps
        /// simulation. This includes variable-timesteps animations, input
        /// processing, and system maintenance that adapts to frame rate.
        ///
        /// @note Called one per frame from the main thread after fixed updates
        /// @note Frame time is variable and depends on rendering performance
        void OnUpdate(float deltaTime) override;

        /// @brief Performs rendering operations for the current frame
        /// @param deltaTime Time elapsed since the last frame in seconds
        ///
        /// Executes the complete rendering pipeline for the current frame including
        /// all GPU operations, command submission, and frame presentation. This is
        /// the primary client-specific application update that distinguishes it
        /// from server applications.
        ///
        /// @note Called once per frame after `OnUpdate()`
        void OnApplicationUpdate(float deltaTime) override;

        /// @brief Processes client-specific events(window, input, rendering)
        /// @param e Event to process
        ///
        /// Extends the base event processing to handle client-specific events
        /// including window management, input processing, and rendering events.
        void OnEvent(Event& e) override;

        /// @brief Handles window close events with proper shutdown initiation
        /// @param e Window close event data
        /// @return true if the event was handled, false otherwise
        ///
        /// Processes window close requests from the user or system. Provides
        /// an opportunity for graceful shutdown procedures, save dialogs,
        /// or shutdown cancellation based on application state.
        virtual bool OnWindowClose(WindowCloseEvent& e);

        /// @brief Handles window resize events with rendering surface updates
        /// @param e Window resize event containing new dimensions
        /// @return true if the event was handled, false otherwise
        ///
        /// Manages rendering surface resize operations when the window size
        /// changes. This includes updating render targets, swap chains, camera
        /// aspect ratios, and UI layouts to match new window dimensions.
        virtual bool OnWindowResize(WindowResizedEvent& e);

        /// @brief Handles key press events
        /// @param e Key press event containing key information
        /// @return true if the event was handled, false otherwise
        ///
        /// Processes client-specific keyboard input. Including but not limited to
        /// debug shortcuts, application controls, ...
        ///
        /// **Default key bindings:**
        /// - ESC: Application termination
        /// - 0: Normal rendering mode (full lighting and materials)
        /// - 1: Lighting debug mode (visualizes lighting calculations)
        /// - 2: Normals debug mode (virtualizes surface normals)
        virtual bool OnKeyPressed(KeyPressedEvent& e);

        /// @brief Main window instance managed by the client application
        ///
        /// The window serves as the primary interface between the application
        /// and the operating system's windowing system. It provides the rendering
        /// surface and generates input/window events for application processing.
        std::unique_ptr<Window> m_MainWindow;
    };
} // VoidArchitect
