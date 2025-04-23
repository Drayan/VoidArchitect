use winit::application::ApplicationHandler;
use winit::event::WindowEvent;
use winit::event_loop::{ActiveEventLoop, EventLoop};
use winit::window::{Window, WindowAttributes, WindowId};

use crate::EngineApplication;

/// System responsible for platform abstraction and windowing.
///
/// # Platform Limitation
/// On macOS, `winit::EventLoop` can only be created on the main thread. This means that tests
/// creating an EventLoop will fail on macOS due to platform restrictions. See integration test
/// documentation for details.
///
/// PlatformSystem manages initialization, running, and shutdown phases for the windowing
/// event loop.
pub struct PlatformLayer<A: EngineApplication> {
    title: String,
    event_loop: Option<EventLoop<()>>,
    window: Option<Window>,
    engine_app: Option<A>,
    // Add more fields as needed for future extensibility
}

impl<A: EngineApplication> ApplicationHandler for PlatformLayer<A> {
    fn resumed(&mut self, event_loop: &ActiveEventLoop) {
        let window = event_loop
            .create_window(WindowAttributes::default().with_title(self.title.clone()))
            .expect("Failed to create window");
        self.window = Some(window);
    }

    fn about_to_wait(&mut self, _event_loop: &ActiveEventLoop) {
        self.engine_app.as_mut().expect("EngineContext must be initialized.").update(0.0);
        if let Some(window) = &self.window {
            window.request_redraw();
        }
    }

    fn window_event(
        &mut self,
        event_loop: &ActiveEventLoop,
        _window_id: WindowId,
        event: WindowEvent,
    ) {
        match event {
            WindowEvent::RedrawRequested => {
                // Handle window redraw
                self.engine_app
                    .as_mut()
                    .expect("EngineContext must be initialized.")
                    .render(0.0);
            }
            WindowEvent::CloseRequested => {
                log::info!("Window close requested");
                event_loop.exit();
            }
            _ => {}
        }
    }
}

impl<A: EngineApplication> PlatformLayer<A> {
    /// Constructs a new PlatformSystem with the given window title, but does not create the
    /// event loop or window yet.
    pub fn new() -> Self {
        Self {
            title: String::from(""),
            event_loop: None,
            window: None,
            engine_app: None,
        }
    }

    /// Initializes the event loop and prepares the system for running.
    ///
    /// # Arguments
    /// * `title` - The title of the window to be created.
    pub fn initialize(&mut self, title: &str) {
        self.event_loop = Some(EventLoop::new().unwrap());
        self.title = title.to_string();
        // Window is created in resumed(), see ApplicationHandler impl
    }

    /// Runs the main event loop. This will block until the event loop exits.
    pub fn run(&mut self, engine_app: A) {
        let event_loop =
            self.event_loop.take().expect("Event loop must be initialized before run()");

        // Set the engine application reference
        self.engine_app = Some(engine_app);
        event_loop.run_app(self).expect("Failed to run event loop");
    }

    // Cleans up resources if necessary (placeholder for future extensibility).
    pub fn shutdown(&mut self) {
        // No-op for now
    }
}

// Unit tests should be placed in tests/platform.rs (see project conventions)
