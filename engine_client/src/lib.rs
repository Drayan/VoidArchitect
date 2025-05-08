//! Engine client library root module.
//!
//! This module re-exports the main subsystems of the engine client, including platform abstraction
//! and rendering. It defines the main entry points for engine context, application trait, and
//! orchestrates subsystem initialization and shutdown.

pub mod network;
pub mod platform;
pub mod renderer;

use network::NetworkSystem;
use platform::{PlatformLayer, WindowHandle};
use renderer::RendererFrontend;
use std::error::Error;
use std::fmt;
use tokio::runtime::Runtime;

#[derive(Debug)]
pub enum EngineError {
    RendererError(String),
    NetworkError(anyhow::Error),
    PlatformError(String),
    InitializationError(String),
    ShutdownError(String),
    UpdateError(String),
    RenderError(String),
    ResizeError(String),
}

impl fmt::Display for EngineError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            EngineError::RendererError(e) => write!(f, "Renderer error: {}", e),
            EngineError::NetworkError(e) => write!(f, "Network error: {}", e),
            EngineError::PlatformError(e) => write!(f, "Platform error: {}", e),
            EngineError::InitializationError(e) => write!(f, "Initialization error: {}", e),
            EngineError::ShutdownError(e) => write!(f, "Shutdown error: {}", e),
            EngineError::UpdateError(e) => write!(f, "Update error: {}", e),
            EngineError::RenderError(e) => write!(f, "Render error: {}", e),
            EngineError::ResizeError(e) => write!(f, "Resize error: {}", e),
        }
    }
}

impl Error for EngineError {}

/// Holds the main state for the engine, including the renderer system.
///
/// This struct is used to manage the core engine context and its subsystems.
pub struct EngineContext {
    /// The renderer frontend system, if initialized.
    // Holds the renderer frontend system; None if not initialized.
    is_initialized: bool,
    renderer_system: Option<RendererFrontend>,
    network_system: Option<NetworkSystem>,
}

impl EngineContext {
    /// Creates a new engine context with a default renderer system.
    ///
    /// # Returns
    /// * `EngineContext` - A new engine context instance.
    pub fn new() -> Self {
        EngineContext {
            renderer_system: Some(RendererFrontend::new()),
            network_system: Some(NetworkSystem::new()),
            is_initialized: false,
        }
    }

    // Trait for internal engine application lifecycle management.
    /// Initializes the application with the given window handle.
    ///
    /// # Arguments
    /// * `window` - The handle to the window provided by the platform layer.
    ///
    /// # Returns
    /// * `Ok(())` if initialization succeeds, or `Err(String)` with an error message.
    pub async fn initialize(&mut self, window: WindowHandle) -> Result<(), EngineError> {
        // Initialize the network system.
        if let Some(network) = &mut self.network_system {
            match network.initialize().await.map_err(EngineError::NetworkError) {
                Ok(_) => log::info!("Network system initialized successfully"),
                Err(e) => {
                    log::error!("Failed to initialize network system: {}", e);
                    return Err(EngineError::InitializationError(
                        "Failed to initialize network system".to_string(),
                    ));
                }
            }
        } else {
            return Err(EngineError::InitializationError(
                "Network system not initialized".to_string(),
            ));
        }

        // Initialize the renderer system with the provided window handle.
        if let Some(renderer) = &mut self.renderer_system {
            match renderer.initialize(window).map_err(EngineError::RendererError) {
                Ok(_) => {
                    log::info!("Renderer system initialized successfully");
                    self.is_initialized = true;
                }
                Err(e) => {
                    log::error!("Failed to initialize renderer system: {}", e);
                    return Err(EngineError::InitializationError(
                        "Failed to initialize renderer system".to_string(),
                    ));
                }
            }
        } else {
            return Err(EngineError::InitializationError(
                "Renderer system not initialized".to_string(),
            ));
        }

        Ok(())
    }

    /// Shuts down the engine context and releases resources.
    ///
    /// # Returns
    /// * `Ok(())` if shutdown succeeds, or `Err(String)` with an error message.
    pub async fn shutdown(&mut self) -> Result<(), EngineError> {
        if !self.is_initialized {
            return Err(EngineError::InitializationError(
                "Engine context not initialized".to_string(),
            ));
        }

        if let Some(renderer) = &mut self.renderer_system {
            renderer.shutdown().map_err(EngineError::RendererError)?;
        } else {
            return Err(EngineError::InitializationError(
                "Renderer system not initialized".to_string(),
            ));
        }

        if let Some(network) = &mut self.network_system {
            network.shutdown().await.map_err(EngineError::NetworkError)?;
        } else {
            return Err(EngineError::InitializationError(
                "Network system not initialized".to_string(),
            ));
        }

        Ok(())
    }

    /// Updates the engine context state.
    ///
    /// # Arguments
    /// * `delta_time` - Time elapsed since the last update, in seconds.
    ///
    /// # Returns
    /// * `Ok(())` if update succeeds, or `Err(String)` with an error message.
    pub fn update(&mut self, _delta_time: f32) -> Result<(), EngineError> {
        if !self.is_initialized {
            return Err(EngineError::InitializationError(
                "Engine context not initialized".to_string(),
            ));
        }
        let renderer = self.renderer_system.as_mut().ok_or(
            EngineError::InitializationError("Renderer system not initialized".to_string()),
        )?;
        // Default implementation does nothing.
        // Override in  if needed.
        // For now, just return Ok, but propagate errors if update logic is added.
        Ok(())
    }

    /// Renders the engine context.
    ///
    /// # Arguments
    /// * `delta_time` - Time elapsed since the last render, in seconds.
    ///
    /// # Returns
    /// * `Ok(())` if render succeeds, or `Err(String)` with an error message.
    pub fn render(&mut self, _delta_time: f32) -> Result<(), EngineError> {
        if !self.is_initialized {
            return Err(EngineError::InitializationError(
                "Engine context not initialized".to_string(),
            ));
        }
        let renderer = self.renderer_system.as_mut().ok_or(
            EngineError::InitializationError("Renderer system not initialized".to_string()),
        )?;

        // Begin a new rendering frame.
        match renderer.begin_frame().map_err(EngineError::RendererError) {
            Ok(proceed) => {
                if proceed {
                    // Rendering logic for the application should be inserted here.
                    renderer.end_frame().map_err(EngineError::RendererError)?;
                }
            }
            Err(e) => {
                log::error!("Failed to begin frame: {}", e);
                return Err(EngineError::RenderError(
                    "Failed to begin rendering frame".to_string(),
                ));
            }
        }

        Ok(())
    }

    /// Handles window resizing.
    ///
    /// # Arguments
    /// * `width` - The new width of the window.
    /// * `height` - The new height of the window.
    ///
    /// # Returns
    /// * `Ok(())` if resize succeeds, or `Err(String)` with an error message.
    pub fn resize(&mut self, width: u32, height: u32) -> Result<(), EngineError> {
        if !self.is_initialized {
            return Err(EngineError::InitializationError(
                "Engine context not initialized".to_string(),
            ));
        }

        if let Some(renderer) = &mut self.renderer_system {
            renderer.resize(width, height).map_err(EngineError::RendererError)
        } else {
            Err(EngineError::InitializationError(
                "Renderer system not initialized".to_string(),
            ))
        }
    }
}

/// Main entry point for the client engine. Owns the platform system and other subsystems.
/// This struct is responsible for subsystem orchestration.
/// Main entry point for the client engine. Owns the platform system and other subsystems.
///
/// This struct is responsible for subsystem orchestration.
pub struct EngineClient {
    runtime: Runtime,
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
        let runtime = tokio::runtime::Builder::new_multi_thread()
            .worker_threads(4)
            .enable_all()
            .build()
            .expect("Failed to create Tokio runtime");
        EngineClient {
            runtime,
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
        self.runtime
            .block_on(self.context.initialize(window))
            .expect("Failed to initialize context");
    }

    /// Runs the main event loop. This will block until the event loop exits.
    ///
    /// # Returns
    /// * `Self` - The engine client after the event loop has run.
    pub fn run(&mut self) -> Result<(), String> {
        // Main event loop: runs until the platform layer signals to quit.
        self.runtime.block_on(async {
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
        });

        Ok(())
    }

    /// Shuts down the engine subsystems.
    pub fn shutdown(&mut self) {
        // Shutdown the engine context and platform layer.
        self.runtime.block_on(self.context.shutdown()).expect("Failed to shutdown context");
        self.platform_layer.shutdown();
        log::info!("Engine shutdown");
    }
}
