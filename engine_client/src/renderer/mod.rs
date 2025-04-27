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
    pub backend: Box<dyn RendererBackend>,
}

impl RendererFrontend {
    pub fn new() -> Self {
        Self {
            //TODO: This should be configurable
            // By default, use the Vulkan backend. This should be configurable in the future.
            backend: Box::new(RendererBackendVulkan::new()),
        }
    }

    pub fn initialize(&mut self, window: WindowHandle) -> Result<(), String> {
        // Check if we have a valid window handle
        if window.window.as_ptr().is_null() {
            return Err("Invalid window handle".to_string());
        }
        // Initialize the backend with the provided window handle
        self.backend.initialize(window)
    }

    pub fn shutdown(&mut self) -> Result<(), String> {
        self.backend.shutdown()
    }

    pub fn begin_frame(&mut self) -> Result<(), String> {
        self.backend.begin_frame()
    }

    pub fn end_frame(&mut self) -> Result<(), String> {
        self.backend.end_frame()
    }

    pub fn resize(&mut self, width: u32, height: u32) -> Result<(), String> {
        self.backend.resize(width, height)
    }
}
