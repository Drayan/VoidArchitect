//
// Created by Michael Desmedt on 12/05/2025.
//
#pragma once

#include "Core.hpp"

/// @file Application.hpp
/// @brief Core application base class for VoidArchitect engine applications
///
/// This file defines the foundational Application class that serves as the base
/// for all VoidArchitect application types. It provides essential lifecycle management,
/// job system integration, and the main execution loop infrastructure.
///
/// **Application architecture:**
/// - Application (base): Core loop, job system, lifecycle
/// - ClientApplication: + Rendering, windowing, input
/// - ServerApplication: + Networking, headless operation
/// - EditorApplication: + Content creation tools

namespace VoidArchitect::Platform
{
    class IThread;
}

namespace VoidArchitect
{
    namespace Events
    {
        class Event;
    }

    /// @brief  Base application class for all VoidArchitect application types
    ///
    /// Application provides the foundational infrastructure common to all application
    /// types in the VoidArchitect ecosystem. This includes the main execution loop,
    /// job system integration, layer management, and core lifecycle management.
    ///
    /// The class is designed to be platform-agnostic and UI-agnostic, making it
    /// suitable for both client applications (with rendering) and server applications
    /// (headless). Derived classes add specific functionality for their use cases.
    ///
    /// **Core Responsabilities:**
    /// - Main loop execution and fixed timestep updates
    /// - Job system integration for parallel task execution
    /// - Event system for decoupled communication
    /// - Application lifecycle management
    ///
    /// **Application Hierarchy:**
    /// - Application (base): Main loop, jobs, events
    /// - ClientApplication: + Window, rendering, input handling
    /// - ServerApplication: + Networking, game logic, database
    /// - EditorApplication: + Editor tools, asset management
    ///
    /// @see `ClientApplication` for rendering-enabled applications
    /// @see `ServerApplication` for headless server implementations
    /// @see `EditorApplication` for content creation tools
    class VA_API Application
    {
    public:
        /// @brief Construct the base application instance
        ///
        /// @warning Main thread only - creates application instance
        ///
        /// Constructs the base application with default initialisation.
        /// Two-phase construction pattern requires calling Initialize()
        /// after construction to complete setup.
        ///
        /// @note Call `Initialize()` after construction to complete setup
        Application() = default;

        /// @brief Virtual destructor ensuring proper clean-up of derived classes
        ///
        /// @warning Main thread only - performs application shutdown
        ///
        /// Calls ShutdownSubsystems() to allow derived classes to perform clean-up
        /// before the base class resources are released. This ensures a proper
        /// shutdown order and prevents resource leaks.
        ///
        /// **Shutdown order:**
        /// 1. Call ShutdownSubsystems() for derived class clean-up
        /// 2. Clean up job system
        /// 3. Base class clean-up
        virtual ~Application();

        /// @brief Two-phase initialisation method that *must* be called after construction
        ///
        /// @warning Main thread only - performs system initialisation
        ///
        /// Completes the application initialisation process by setting up all required
        /// systems in the proper order.
        ///
        /// **Initialisation sequence:**
        /// 1. Initialise job system
        /// 2. Call `InitializeSubsystems()`
        /// 3. Call `OnInitialized()` hook
        ///
        /// **Usage:**
        /// @code
        /// auto app = new ClientApplication();
        /// app->Initialize(); // Complete initialisation
        /// app->Run(); // Start the main loop
        /// @endcode
        ///
        /// @throws std::exception if any subsystem initialisation fails
        void Initialize();

        /// @brief Main execution loop with fixed timestep updates
        ///
        /// @warning Main thread only - must be called from the main thread
        ///
        /// Implements the core application loop suitable for all application types.
        /// The loop provides consistent timing, job processing, and updates
        /// while delegating application-specific behaviour to virtual methods.
        ///
        /// **Loop structure:**
        /// 1. Process main thread jobs with time budget management
        /// 2. Accumulate frame time for fixed timestep simulation
        /// 3. Execute fixed updates at 60 FPS via OnFixedUpdate()
        /// 4. Call OnUpdate() hook for derived class frame logic
        /// 5. Call OnLogic() for application-specific updates
        ///
        /// **Termination:**
        /// The loop continues until m_Running is set to false, either by
        /// external events or internal application logic.
        ///
        /// @note This method blocks until the application is terminated
        void Run();

    protected:
        /// @brief Virtual method for derived classes to initialise their subsystems
        ///
        /// @warning Main thread only - called during initialisation phase
        ///
        /// Called during initialisation after core systems have been initialised.
        /// Derived classes should initialise their specific subsystems here while
        /// maintaining proper dependency order.
        ///
        /// **Implementation requirements:**
        /// - Must be implemented by all derived classes
        /// - Should initialise application-specific systems
        /// - Must handle initialisation failures gracefully
        /// - Should respect dependency ordering
        ///
        /// **Common subsystems:**
        /// - Window and rendering systems (ClientApplication)
        /// - Network and database systems (ServerApplication)
        /// - Asset and tool systems (EditorApplication)
        virtual void InitializeSubsystems() = 0;

        /// @brief Virtual hook called after successful initialisation
        ///
        /// @warning Main thread only - called during initialisation phase
        ///
        /// Called after all core systems and subsystems have been successfully
        /// initialised. Override in derived classes to perform final setup tasks
        /// that depend on all systems being ready.
        ///
        /// **Common use cases:**
        /// - Load initial application state
        /// - Set up application-specific event handlers
        /// - Perform initial asset loading
        /// - Configure application settings
        virtual void OnInitialized()
        {
        };

        /// @brief Virtual hook called when application shutdown is requested
        ///
        /// @warning Main thread only - called during shutdown initiation
        ///
        /// Called when the application begins its shutdown sequence, allowing
        /// derived classes to perform early cleanup tasks before system shutdown.
        ///
        /// **Common use cases:**
        /// - Save application state and user data
        /// - Display shutdown confirmation dialogs
        /// - Cancel ongoing operations
        /// - Prepare for graceful termination
        virtual void OnShutdownRequested()
        {
        };

        /// @brief Virtual method for derived classes to perform per-frame updates
        /// @param deltaTime Time elapsed since the last frame in seconds
        ///
        /// @warning Main thread only - called from the main application loop
        ///
        /// Called once per frame after fixed timestep updates have been processed.
        /// This is where derived classes should implement frame-specific logic
        /// that requires variable timestep handling.
        ///
        /// **Performance considerations:**
        /// - Keep processing lightweight to maintain frame rate
        /// - Use JobSystem for heavy computational tasks
        /// - Consider frame time budgets for consistent performance
        virtual void OnUpdate(float deltaTime)
        {
        };

        /// @brief Virtual method for derived classes to perform application-specific updates
        /// @param deltaTime Time elapsed since the last frame in seconds
        ///
        /// @warning Main thread only - called from the main application loop
        ///
        /// Called once per frame after `OnUpdate()` to allow derived classes to perform
        /// their primary application-specific work. This separation allows for clear
        /// distinction between general updates and core application functionality.
        ///
        /// **Implementation requirements:**
        /// - Must be implemented by all derived classes
        /// - Should contain the core application logic
        /// - Consider performance impact on frame rate
        virtual void OnLogic(float deltaTime) = 0;

        /// @brief Virtual method for derived classes to handle fixed timestep updates
        /// @param fixedDeltaTime Fixed timestep duration in seconds
        ///
        /// @warning Main thread only - called from the fixed timestep loop
        ///
        /// Called during fixed timestep simulation loop. Derived classes should
        /// implement game logic, physics, and other systems requiring consistent timing.
        ///
        /// **Use cases:**
        /// - Physics simulation updates
        /// - Game logic that requires deterministic timing
        /// - Animation systems with fixed frame rates
        /// - Network synchronisation logic
        virtual void OnFixedUpdate(float fixedDeltaTime)
        {
        };

        /// @brief Application running state flag
        ///
        /// @warning Main thread access only - not thread-safe
        ///
        /// Controls the main loop execution. When set to false, the `Run()` method
        /// will complete its current iteration and then exit, terminating the
        /// application gracefully.
        ///
        /// **Access patterns:**
        /// - Read/write from main thread only
        /// - Modified during shutdown sequences
        /// - Checked each frame in the main loop
        bool m_Running = true;
    };

    /// @brief Factory function for creating application instances
    /// @return Pointer to the created application instance
    ///
    /// @warning Must be implemented by client code - pure factory function
    ///
    /// This function must be implemented by the specific application to provide
    /// the concrete application type that should be instantiated. It serves as
    /// the main entry point for application creation.
    ///
    /// **Implementation requirements:**
    /// - Must return a valid Application-derived instance
    /// - Should handle memory allocation appropriately
    /// - Called by the engine's main entry point
    ///
    /// **Usage:**
    /// @code
    /// VoidArchitect::Application* VoidArchitect::CreateApplication()
    /// {
    ///     return new ClientApplication();
    /// }
    /// @endcode
    Application* CreateApplication();
} // namespace VoidArchitect
