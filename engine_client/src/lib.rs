//! Engine client library root module.
//!
//! This module re-exports the main subsystems of the engine client, including platform abstraction
//! and rendering. It defines the main entry points for engine context, application trait, and
//! orchestrates subsystem initialization and shutdown.

pub mod platform;
pub mod renderer;

use platform::{PlatformLayer, WindowHandle};
use renderer::RendererFrontend;

pub struct EngineContext {
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

pub trait EngineApplication {
    fn initialize(&mut self, window: WindowHandle) -> Result<(), String>;
    fn shutdown(&mut self) -> Result<(), String>;

    fn update(&mut self, delta_time: f32);
    fn render(&mut self, delta_time: f32);

    fn resize(&mut self, width: u32, height: u32) -> Result<(), String>;
}

impl EngineApplication for EngineContext {
    fn initialize(&mut self, window: WindowHandle) -> Result<(), String> {
        if let Some(renderer) = &mut self.renderer_system {
            renderer.initialize(window)
        } else {
            Err("Renderer system not initialized".to_string())
        }
    }

    fn shutdown(&mut self) -> Result<(), String> {
        if let Some(renderer) = &mut self.renderer_system {
            renderer.shutdown()
        } else {
            Err("Renderer system not initialized".to_string())
        }
    }

    fn update(&mut self, _delta_time: f32) {
        // Default implementation does nothing
    }

    fn render(&mut self, _delta_time: f32) {
        self.renderer_system
            .as_mut()
            .expect("Renderer system not initialized")
            .begin_frame()
            .expect("Failed to begin frame");

        // TODO: Renderer logic should be here

        self.renderer_system
            .as_mut()
            .expect("Renderer system not initialized")
            .end_frame()
            .expect("Failed to end frame");
    }

    fn resize(&mut self, width: u32, height: u32) -> Result<(), String> {
        if let Some(renderer) = &mut self.renderer_system {
            renderer.resize(width, height)
        } else {
            Err("Renderer system not initialized".to_string())
        }
    }
}

/// Main entry point for the client engine. Owns the platform system and other subsystems.
/// This struct is responsible for subsystem orchestration.
pub struct EngineClient {
    platform_layer: PlatformLayer<EngineContext>,
    context: EngineContext,
}

impl<'a> EngineClient {
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
    pub fn initialize(&mut self, title: &str, window_width: u32, window_height: u32) {
        self.platform_layer.initialize(title, window_width, window_height);
    }

    /// Runs the main event loop. This will block until the event loop exits.
    ///
    /// # Returns
    /// * `Self` - The engine client after the event loop has run.
    pub fn run(mut self) -> Self {
        self.context = self.platform_layer.run(self.context);
        self
    }

    /// Shuts down the engine subsystems.
    pub fn shutdown(&mut self) {
        self.context.shutdown().expect("Failed to shutdown context");
        self.platform_layer.shutdown();
        log::info!("Engine shutdown");
    }
}
