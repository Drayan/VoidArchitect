//! Engine client library root module.
//!
//! This module re-exports the main subsystems of the engine client, including platform abstraction
//! and rendering. It defines the main entry points for engine context, application trait, and
//! orchestrates subsystem initialization and shutdown.

pub mod platform;
pub mod renderer;

use platform::{PlatformLayer, WindowHandle};
use renderer::RendererFrontend;

/// Holds the main state for the engine, including the renderer system.
///
/// This struct is used to manage the core engine context and its subsystems.
pub struct EngineContext {
    /// The renderer frontend system, if initialized.
    // Holds the renderer frontend system; None if not initialized.
    renderer_system: Option<RendererFrontend>,
}

impl EngineContext {
    /// Creates a new engine context with a default renderer system.
    ///
    /// # Returns
    /// * `EngineContext` - A new engine context instance.
    pub fn new() -> Self {
        EngineContext {
            renderer_system: Some(RendererFrontend::new()),
        }
    }
}

/// Trait for applications using the engine client.
///
/// Implement this trait to define the lifecycle and rendering logic for your application.
pub(crate) trait EngineApplication {
    // Trait for internal engine application lifecycle management.
    /// Initializes the application with the given window handle.
    ///
    /// # Arguments
    /// * `window` - The handle to the window provided by the platform layer.
    ///
    /// # Returns
    /// * `Ok(())` if initialization succeeds, or `Err(String)` with an error message.
    fn initialize(&mut self, window: WindowHandle) -> Result<(), String>;

    /// Shuts down the application and releases resources.
    ///
    /// # Returns
    /// * `Ok(())` if shutdown succeeds, or `Err(String)` with an error message.
    fn shutdown(&mut self) -> Result<(), String>;

    /// Updates the application state.
    ///
    /// # Arguments
    /// * `delta_time` - Time elapsed since the last update, in seconds.
    fn update(&mut self, delta_time: f32);

    /// Renders the application.
    ///
    /// # Arguments
    /// * `delta_time` - Time elapsed since the last render, in seconds.
    fn render(&mut self, delta_time: f32);

    /// Handles window resizing.
    ///
    /// # Arguments
    /// * `width` - The new width of the window.
    /// * `height` - The new height of the window.
    ///
    /// # Returns
    /// * `Ok(())` if resize succeeds, or `Err(String)` with an error message.
    fn resize(&mut self, width: u32, height: u32) -> Result<(), String>;
}

impl EngineApplication for EngineContext {
    fn initialize(&mut self, window: WindowHandle) -> Result<(), String> {
        // Initialize the renderer system with the provided window handle.
        if let Some(renderer) = &mut self.renderer_system {
            renderer.initialize(window)
        } else {
            Err("Renderer system not initialized".to_string())
        }
    }

    fn shutdown(&mut self) -> Result<(), String> {
        // Shutdown the renderer system if it exists.
        if let Some(renderer) = &mut self.renderer_system {
            renderer.shutdown()
        } else {
            Err("Renderer system not initialized".to_string())
        }
    }

    fn update(&mut self, _delta_time: f32) {
        // Default implementation does nothing.
        // Override in application-specific context if needed.
    }

    fn render(&mut self, _delta_time: f32) {
        // Begin a new rendering frame.
        self.renderer_system
            .as_mut()
            .expect("Renderer system not initialized")
            .begin_frame()
            .expect("Failed to begin frame");

        // Rendering logic for the application should be inserted here.

        self.renderer_system
            .as_mut()
            .expect("Renderer system not initialized")
            .end_frame()
            .expect("Failed to end frame");
    }

    fn resize(&mut self, width: u32, height: u32) -> Result<(), String> {
        // Resize the renderer system if it exists.
        if let Some(renderer) = &mut self.renderer_system {
            renderer.resize(width, height)
        } else {
            Err("Renderer system not initialized".to_string())
        }
    }
}

/// Main entry point for the client engine. Owns the platform system and other subsystems.
/// This struct is responsible for subsystem orchestration.
/// Main entry point for the client engine. Owns the platform system and other subsystems.
///
/// This struct is responsible for subsystem orchestration.
pub struct EngineClient {
    /// The platform abstraction layer (window, events, etc).
    // Platform abstraction layer (window, events, etc).
    platform_layer: PlatformLayer,
    /// The main engine context, including renderer and state.
    // Main engine context, including renderer and state.
    context: EngineContext,
}

impl EngineClient {
    /// Creates a new engine client with default platform and context.
    ///
    /// # Returns
    /// * `EngineClient` - A new engine client instance.
    pub fn new() -> Self {
        EngineClient {
            platform_layer: PlatformLayer::new(),
            context: EngineContext::new(),
        }
    }

    /// Initialize the engine subsystems.
    ///
    /// # Arguments
    /// * `title` - The title of the window to be created.
    /// * `window_width` - The width of the window.
    /// * `window_height` - The height of the window.
    ///
    /// # Panics
    /// This function will panic if the platform layer fails to initialize or if the context fails to initialize.
    pub fn initialize(&mut self, title: &str, window_width: u32, window_height: u32) {
        // Initialize the platform layer (window, input, etc).
        self.platform_layer
            .initialize(title, window_width, window_height)
            .expect("Failed to initialize platform layer");

        // Retrieve the window handle from the platform layer.
        let window = match self.platform_layer.get_window_handle() {
            Some(handle) => handle,
            None => panic!("Failed to get window handle"),
        };
        // Initialize the engine context with the window handle.
        self.context.initialize(window).expect("Failed to initialize context");
    }

    /// Runs the main event loop. This will block until the event loop exits.
    ///
    /// # Returns
    /// * `Self` - The engine client after the event loop has run.
    pub fn run(&mut self) -> Result<(), String> {
        // Main event loop: runs until the platform layer signals to quit.
        loop {
            match self.platform_layer.run() {
                Ok(should_quit) => {
                    if should_quit {
                        break;
                    }
                }
                Err(e) => {
                    log::error!("Error in platform layer: {}", e);
                    break;
                }
            }

            // TODO: Calculate delta time for frame timing.
            // Render the frame (fixed timestep for now).
            self.context.render(0.016); // ~60fps
            // Update the context (fixed timestep for now).
            self.context.update(0.016); // ~60fps
        }

        Ok(())
    }

    /// Shuts down the engine subsystems.
    pub fn shutdown(&mut self) {
        // Shutdown the engine context and platform layer.
        self.context.shutdown().expect("Failed to shutdown context");
        self.platform_layer.shutdown();
        log::info!("Engine shutdown");
    }
}
