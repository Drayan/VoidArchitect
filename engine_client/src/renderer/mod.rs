//! Renderer frontend and backend abstraction for the engine client.
//!
//! This module defines the `RendererFrontend` struct and the `RendererBackend` trait, which provide
//! an interface for rendering operations. The backend can be implemented for different graphics APIs
//! (e.g., Vulkan). The frontend delegates rendering calls to the selected backend implementation.

pub mod backends;

use crate::{platform::WindowHandle, renderer::backends::vulkan::RendererBackendVulkan};

pub trait RendererBackend {
    /// Initializes the renderer backend with the given window handle.
    ///
    /// # Arguments
    /// * `window` - The window handle for surface creation.
    ///
    /// # Returns
    /// * `Ok(())` if initialization succeeds.
    /// * `Err(String)` if an error occurs.
    fn initialize(&mut self, window: WindowHandle) -> Result<(), String>;

    /// Shuts down the renderer backend and releases resources.
    ///
    /// # Returns
    /// * `Ok(())` if shutdown succeeds.
    /// * `Err(String)` if an error occurs.
    fn shutdown(&mut self) -> Result<(), String>;

    /// Begins a new rendering frame.
    ///
    /// # Returns
    /// * `Ok(())` if the frame begins successfully.
    /// * `Err(String)` if an error occurs.
    fn begin_frame(&mut self) -> Result<(), String>;

    /// Ends the current rendering frame.
    ///
    /// # Returns
    /// * `Ok(())` if the frame ends successfully.
    /// * `Err(String)` if an error occurs.
    fn end_frame(&mut self) -> Result<(), String>;

    /// Handles window resizing for the renderer backend.
    ///
    /// # Arguments
    /// * `width` - The new width of the window.
    /// * `height` - The new height of the window.
    ///
    /// # Returns
    /// * `Ok(())` if resizing succeeds.
    /// * `Err(String)` if an error occurs.
    fn resize(&mut self, width: u32, height: u32) -> Result<(), String>;
}

pub struct RendererFrontend {
    // Holds the selected renderer backend implementation (e.g., Vulkan).
    backend: Box<dyn RendererBackend>,
    is_initialized: bool,
}

impl RendererFrontend {
    pub fn new() -> Self {
        Self {
            //TODO: This should be configurable
            // By default, use the Vulkan backend. This should be configurable in the future.
            backend: Box::new(RendererBackendVulkan::new()),
            is_initialized: false,
        }
    }

    pub fn initialize(&mut self, window: WindowHandle) -> Result<(), String> {
        let result = self.backend.initialize(window);
        if result.is_ok() {
            self.is_initialized = true;
        }
        result
    }

    pub fn shutdown(&mut self) -> Result<(), String> {
        if !self.is_initialized {
            return Err("Renderer not initialized".to_string());
        }
        self.is_initialized = false;
        self.backend.shutdown()
    }

    pub fn begin_frame(&mut self) -> Result<(), String> {
        if !self.is_initialized {
            return Err("Renderer not initialized".to_string());
        }
        self.backend.begin_frame()
    }

    pub fn end_frame(&mut self) -> Result<(), String> {
        if !self.is_initialized {
            return Err("Renderer not initialized".to_string());
        }
        self.backend.end_frame()
    }

    pub fn resize(&mut self, width: u32, height: u32) -> Result<(), String> {
        if !self.is_initialized {
            return Err("Renderer not initialized".to_string());
        }
        self.backend.resize(width, height)
    }
}
