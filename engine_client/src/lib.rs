mod platform;
use platform::PlatformSystem;

pub struct EngineClient {
    platform_system: Option<PlatformSystem>,
}

impl EngineClient {
    pub fn new() -> Self {
        EngineClient {
            platform_system: None,
        }
    }

    pub fn initialize(&mut self) {
        // Initialize the platform system
        self.platform_system = Some(PlatformSystem::new());
        self.platform_system
            .as_mut()
            .expect("platform should exist")
            .initialize();

        println!("EngineClient initialized");
    }

    pub fn run(&self) {
        // Placeholder for the actual run logic
        println!("EngineClient is running");
    }

    pub fn shutdown(&self) {
        // Placeholder for the actual shutdown logic
        println!("EngineClient is shutting down");
    }
}
