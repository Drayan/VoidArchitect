#![doc = r#"
Platform abstraction layer for the engine client.

This module provides the `PlatformLayer` struct and related types for window management,
event handling, and SDL3 integration. It abstracts platform-specific details and exposes
a unified API for the rest of the engine.
"#]

use std::{
    error::Error,
    fmt,
    sync::{Arc, RwLock, Weak},
};

/// A handle providing non-owning access to the SDL window.
/// Used to prevent reference cycles.
#[derive(Clone)]
pub struct WindowHandle {
    // Made public
    /// A weak reference to the SDL window, wrapped in Arc/RwLock for thread safety.
    pub window: Weak<RwLock<sdl3::video::Window>>,
}

/// Manages the platform-specific operations, primarily window creation and event handling using SDL3.
pub struct PlatformLayer {
    // Made public
    /// The title displayed on the window.
    title: String,
    /// The current width of the window's client area.
    window_width: u32,
    /// The current height of the window's client area.
    window_height: u32,
    /// A thread-safe reference-counted pointer to the SDL window. `None` if not initialized.
    window: Option<Arc<RwLock<sdl3::video::Window>>>,
    /// The main SDL context handle. `None` if not initialized.
    sdl_context: Option<sdl3::Sdl>,
    // TODO: Add sender for propagating events (e.g., mpsc::Sender<EngineEvent>)
}

/// Errors that can occur within the PlatformLayer.
#[derive(Debug)]
pub enum PlatformError {
    /// Failed to initialize the main SDL context.
    SdlInit(String),
    /// Failed to initialize the SDL video subsystem.
    VideoSubsystem(String),
    /// Failed to create the SDL window.
    WindowCreation(String),
    /// Failed to get the SDL event pump.
    EventPump(String),
    /// An operation was attempted before the platform layer was initialized.
    NotInitialized(String),
    /// Failed to acquire a lock (RwLock).
    LockError(String),
}

impl fmt::Display for PlatformError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            PlatformError::SdlInit(e) => write!(f, "SDL Initialization Error: {}", e),
            PlatformError::VideoSubsystem(e) => write!(f, "SDL Video Subsystem Error: {}", e),
            PlatformError::WindowCreation(e) => write!(f, "SDL Window Creation Error: {}", e),
            PlatformError::EventPump(e) => write!(f, "SDL Event Pump Error: {}", e),
            PlatformError::NotInitialized(e) => write!(f, "Platform Not Initialized: {}", e),
            PlatformError::LockError(e) => write!(f, "Lock Error: {}", e),
        }
    }
}

impl Error for PlatformError {}

// Helper to convert PoisonError into PlatformError
impl<T> From<std::sync::PoisonError<T>> for PlatformError {
    fn from(err: std::sync::PoisonError<T>) -> Self {
        PlatformError::LockError(err.to_string())
    }
}

// --- Error Mapping Helper Functions ---

/// Helper function for mapping SDL init errors.
fn map_sdl_init_error(e: sdl3::Error) -> PlatformError {
    // Converts an SDL error into a PlatformError for SDL initialization failures.
    PlatformError::SdlInit(e.to_string())
}

/// Helper function for mapping video subsystem errors.
fn map_video_subsystem_error(e: sdl3::Error) -> PlatformError {
    // Converts an SDL error into a PlatformError for video subsystem failures.
    PlatformError::VideoSubsystem(e.to_string())
}

/// Helper function for mapping event pump errors.
fn map_event_pump_error(e: sdl3::Error) -> PlatformError {
    // Converts an SDL error into a PlatformError for event pump failures.
    PlatformError::EventPump(e.to_string())
}

/// Helper function for mapping window creation errors.
fn map_window_creation_error(e: sdl3::video::WindowBuildError) -> PlatformError {
    // Converts a window build error into a PlatformError for window creation failures.
    PlatformError::WindowCreation(e.to_string())
}

impl PlatformLayer {
    /// Creates a new, uninitialized `PlatformLayer` with default settings.
    pub fn new() -> Self {
        log::debug!("Creating new PlatformLayer");
        Self {
            title: "Default Title".to_string(),
            window_width: 1280,
            window_height: 720,
            window: None,
            sdl_context: None,
        }
    }

    /// Initializes the SDL context, video subsystem, and creates the main window.
    ///
    /// # Arguments
    ///
    /// * `title` - The title for the window.
    /// * `width` - The desired width of the window.
    /// * `height` - The desired height of the window.
    ///
    /// # Returns
    ///
    /// * `Ok(())` if initialization is successful.
    /// * `Err(PlatformError)` if any part of the initialization fails.
    pub fn initialize(
        &mut self,
        title: &str,
        width: u32,
        height: u32,
    ) -> Result<(), PlatformError> {
        log::info!(
            "Initializing PlatformLayer with title '{}', size {}x{}",
            title,
            width,
            height
        );
        self.title = title.to_string();
        self.window_width = width;
        self.window_height = height;

        // Initialize SDL context and video subsystem.
        let sdl_context = sdl3::init().map_err(map_sdl_init_error)?;
        log::debug!("SDL context initialized");
        let video_subsystem = sdl_context.video().map_err(map_video_subsystem_error)?;
        log::debug!("SDL video subsystem initialized");

        // --- Window Creation ---
        // Build the window with Vulkan support and other properties.
        // TODO: Make backend flags configurable (e.g., based on renderer choice)
        let mut window_builder = // Make mutable
            video_subsystem.window(&self.title, self.window_width, self.window_height);

        // Assuming Vulkan is the primary target for now based on PLANNING.md
        let mut window = window_builder
            .vulkan() // Enable Vulkan support
            // .metal_view() // Keep Metal disabled unless configured
            .resizable() // Allow window resizing
            .position_centered() // Start window centered
            .build()
            .map_err(map_window_creation_error)?; // Use helper function
        log::debug!("SDL window created");

        window.show();
        log::debug!("SDL window shown");

        // Store window and SDL context in the struct for later use.
        self.window = Some(Arc::new(RwLock::new(window)));
        self.sdl_context = Some(sdl_context);
        log::info!("PlatformLayer initialized successfully");
        Ok(())
    }

    /// Polls for SDL events and processes them.
    ///
    /// This function should be called repeatedly within the main application loop.
    /// It handles window events (like closing or resizing) and input events.
    /// It returns `Ok(true)` if the application should quit.
    ///
    /// # Returns
    ///
    /// * `Ok(true)` if a quit event was received, indicating the application should exit.
    /// * `Ok(false)` if no quit event was received and processing completed normally.
    /// * `Err(PlatformError)` if the SDL context is not initialized or the event pump cannot be obtained.
    pub fn run(&mut self) -> Result<bool, PlatformError> {
        // Ensure SDL context is initialized before polling events.
        let context = self.sdl_context.as_ref().ok_or_else(|| {
            PlatformError::NotInitialized("SDL context not available in run()".to_string())
        })?;

        // Obtain the SDL event pump for polling events.
        let mut event_pump = context.event_pump().map_err(map_event_pump_error)?; // Use helper function

        let mut should_quit = false;
        // Poll and process all pending SDL events.
        for event in event_pump.poll_iter() {
            match event {
                sdl3::event::Event::Quit { .. } => {
                    // User requested application quit (e.g., Cmd+Q or window close).
                    log::info!("Received quit event");
                    should_quit = true;
                }
                sdl3::event::Event::Window { win_event, .. } => {
                    match win_event {
                        sdl3::event::WindowEvent::CloseRequested { .. } => {
                            // Window close button pressed.
                            log::info!("Received window close event");
                            should_quit = true;
                        }
                        sdl3::event::WindowEvent::Resized(width, height) => {
                            // Window was resized by the user.
                            log::info!("Window resized to {}x{}", width, height);
                            self.window_width = width as u32; // Update internal state
                            self.window_height = height as u32;
                            // TODO: Propagate resize event to the renderer/engine
                            // Example: self.event_sender.send(EngineEvent::WindowResized(width, height)).log_err();
                        }
                        // Add other window events as needed (e.g., FocusGained, FocusLost)
                        _ => {}
                    }
                }
                sdl3::event::Event::KeyDown { keycode, .. } => {
                    if let Some(key) = keycode {
                        // Key pressed event received.
                        // log::debug!("Key down: {:?}", key);
                        // TODO: Propagate key down event to input system
                    }
                }
                sdl3::event::Event::KeyUp { keycode, .. } => {
                    if let Some(key) = keycode {
                        // Key released event received.
                        // log::debug!("Key up: {:?}", key);
                        // TODO: Propagate key up event to input system
                    }
                }
                sdl3::event::Event::MouseButtonDown { .. } => {
                    // Mouse button pressed event.
                    // log::debug!("Mouse button down: {:?} at ({}, {})", mouse_btn, x, y);
                    // TODO: Propagate mouse button down event to input system
                }
                sdl3::event::Event::MouseButtonUp { .. } => {
                    // Mouse button released event.
                    // log::debug!("Mouse button up: {:?} at ({}, {})", mouse_btn, x, y);
                    // TODO: Propagate mouse button up event to input system
                }
                sdl3::event::Event::MouseMotion { .. } => {
                    // Mouse moved event.
                    // log::debug!("Mouse motion: at ({}, {}) relative ({}, {})", x, y, xrel, yrel);
                    // TODO: Propagate mouse motion event to input system
                }
                sdl3::event::Event::MouseWheel { .. } => {
                    // Mouse wheel scrolled event.
                    // log::debug!("Mouse wheel: ({}, {}) direction {:?}", x, y, direction);
                    // TODO: Propagate mouse wheel event to input system
                }
                _ => {} // Ignore other event types for now
            }
        }

        Ok(should_quit)
    }

    /// Shuts down the platform layer, hiding the window and releasing SDL resources.
    /// This should be called before the application exits.
    pub fn shutdown(&mut self) {
        log::info!("Shutting down PlatformLayer...");
        if let Some(window_arc) = self.window.take() {
            // Attempt to acquire write lock and hide the window before dropping.
            // This ensures the window is hidden before SDL context is destroyed.
            match window_arc.write() {
                Ok(mut window) => {
                    log::debug!("Hiding window");
                    window.hide();
                }
                Err(e) => log::error!(
                    "Failed to acquire write lock on window during shutdown: {}",
                    e
                ),
            }
            drop(window_arc); // Drop the Arc explicitly to release window resources.
            log::debug!("Window Arc dropped");
        }
        self.sdl_context = None; // Drops the Sdl context, releasing SDL resources.
        log::info!("PlatformLayer shut down successfully.");
    }

    /// Gets a non-owning handle to the window managed by the platform layer.
    ///
    /// This handle provides a `Weak` reference, preventing ownership cycles.
    /// It can be upgraded to an `Arc` temporarily if access to the window is needed.
    /// Primarily used by other systems (like the renderer) to interact with the window
    /// (e.g., for creating a Vulkan surface).
    ///
    /// # Returns
    ///
    /// * `Some(WindowHandle)` if the window has been initialized.
    /// * `None` if the platform layer is not initialized or has been shut down.
    pub fn get_window_handle(&self) -> Option<WindowHandle> {
        self.window.as_ref().map(|arc| WindowHandle {
            window: Arc::downgrade(arc),
        })
    }

    /// Returns the current width of the window based on the latest known size.
    /// This value is updated during resize events.
    /// Returns the current width of the window in pixels.
    ///
    /// This value is updated during resize events and reflects the latest known window size.
    ///
    /// # Returns
    /// * `u32` - The width of the window in pixels.
    pub fn get_window_width(&self) -> u32 {
        self.window_width
    }

    /// Returns the current height of the window based on the latest known size.
    /// This value is updated during resize events.
    /// Returns the current height of the window in pixels.
    ///
    /// This value is updated during resize events and reflects the latest known window size.
    ///
    /// # Returns
    /// * `u32` - The height of the window in pixels.
    pub fn get_window_height(&self) -> u32 {
        self.window_height
    }
}
