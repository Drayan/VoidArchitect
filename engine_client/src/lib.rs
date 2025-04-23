pub mod platform;

use platform::PlatformLayer;

#[derive(Clone)]
pub struct EngineContext {}

impl EngineContext {
    pub fn new() -> Self {
        EngineContext {}
    }
}

pub trait EngineApplication {
    fn update(&mut self, delta_time: f32);
    fn render(&mut self, delta_time: f32);
}

impl EngineApplication for EngineContext {
    fn update(&mut self, _delta_time: f32) {
        // Default implementation does nothing
    }

    fn render(&mut self, _delta_time: f32) {
        // Default implementation does nothing
    }
}

/// Main entry point for the client engine. Owns the platform system and other subsystems.
/// This struct is responsible for subsystem orchestration.
pub struct EngineClient {
    platform_layer: PlatformLayer<EngineContext>,
    context: EngineContext,
}

impl<'a> EngineClient {
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
    pub fn initialize(&mut self, title: &str) {
        self.platform_layer.initialize(title);
    }

    /// Runs the main event loop. This will block until the event loop exits.
    pub fn run(&mut self) {
        self.platform_layer.run(self.context.clone());
    }

    /// Shuts down the engine subsystems.
    pub fn shutdown(&mut self) {
        self.platform_layer.shutdown();
        log::info!("Engine shutdown");
    }
}
