//
// Created by Michael Desmedt on 12/05/2025.
//
#pragma once

#include "Core.hpp"

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
        /// @brief Constructs the base application.
        /// @note Call `Initialize()` after construction to complete setup.
        Application() = default;

        /// @brief Virtual destructor ensuring proper clean-up of derived classes
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

        /// @brief Two-phase initialization method that *must* be called after construction
        ///
        /// Completes the application initialization process by setting up all required
        /// systems in the proper order.
        ///
        /// **Initialization sequence:**
        /// 1. Initialize job system
        /// 1. Call `InitializeSubsystems()`
        ///
        /// **Usage:**
        /// @code
        /// auto app = new ClientApplication();
        /// app->Initialize(); // Complete initialization
        /// app->Run(); // Start the main loop
        /// @endcode
        ///
        /// @throws std::exception if any subsystem initialization fails
        void Initialize();

        /// @brief Main execution loop with fixed timestep updates
        ///
        /// Implements the core application loop suitable for all application types.
        /// The loop provides consistent timing, job processing, and layer updates
        /// while delegating application-specific behaviour to virtual methods.
        ///
        /// **Loop Structure:**
        /// 1. Process main thread jobs with time budget management
        /// 2. Accumulate frame time for fixed timestep simulation
        /// 3. Execute fixed updates for all layers at 60 FPS
        /// 4. Call OnUpdate() hook for derived class frame logic
        /// 5. Call OnApplicationUpdate() for application-specific updates
        ///
        /// @note This method blocks until the application is terminated
        void Run();

    protected:
        /// @brief Virtual method for derived classes to initialize their subsystems
        ///
        /// Called during construction after core systems have been initialized.
        /// Derived classes should initialize their specific subsystems here while
        /// maintaining proper dependency order.
        virtual void InitializeSubsystems() = 0;

        virtual void OnInitialized()
        {
        };

        virtual void OnShutdownRequested()
        {
        };

        /// @brief Virtual method for derived classes to perform per-frame updates
        /// @param deltaTime Time elapsed since the last frame in seconds.
        ///
        /// Called once per frame after fixed timestep updates have been processed.
        /// This is where derived classes should implement frame-specific logic
        /// that require variable timestep handling.
        virtual void OnUpdate(float deltaTime)
        {
        };

        /// @brief Virtual method for derived classes to perform application-specific updates
        /// @param deltaTime Time elapsed since the last frmae in seconds.
        ///
        /// Called once per frame after `OnUpdate()` to allow derived classes to perform
        /// their primary application-specific work. This separation allows for clear
        /// distinction between general updates and core application functionality.
        virtual void OnLogic(float deltaTime) = 0;

        /// @brief Virtual method for derived classes to handle fixed timestep updates
        /// @param fixedDeltaTime Fixed timestep duration in seconds
        ///
        /// Called during fixed timestep simulation loop. Derived classes should
        /// implement game logic, physics, and other systems requiring consistent timing.
        virtual void OnFixedUpdate(float fixedDeltaTime)
        {
        };

        /// @brief Application running state flag
        ///
        /// Controls the main loop execution. When set to false, the `Run()` method
        /// will complete its current iteration and then exit, terminating the
        /// application gracefully.
        ///
        /// @note This should only be modified from the main-thread.
        bool m_Running = true;
    };

    /// @brief Factory function for creating application instances
    /// @return Pointer to the created application instance
    ///
    /// This function must be implemented by the specific application to provide
    /// the concrete application type that should be instantiated. It serves as
    /// the main entry point for application creation.
    ///
    /// Usage:
    /// @code
    /// VoidArchitect::Application* VoidArchitect::CreateApplication()
    /// {
    ///     return new ClientApplication();
    /// }
    /// @endcode
    Application* CreateApplication();
} // namespace VoidArchitect
