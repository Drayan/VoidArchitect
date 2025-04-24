// Integration tests for PlatformSystem and EngineClient.
//
// NOTE: On macOS, `winit::EventLoop` can only be created on the main thread. As a result,
// these tests are skipped on macOS. See platform.rs for more details.

/// Mock implementation of EngineApplication for testing.
struct MockEngineApp {
    pub updated: bool,
    pub rendered: bool,
    pub initialize: bool,
    pub shutdown: bool,
    pub resize: bool,
}

impl Default for MockEngineApp {
    fn default() -> Self {
        Self {
            updated: false,
            rendered: false,
            initialize: false,
            shutdown: false,
            resize: false,
        }
    }
}

impl void_architect_engine_client::EngineApplication for MockEngineApp {
    fn update(&mut self, _dt: f32) {
        self.updated = true;
    }
    fn render(&mut self, _dt: f32) {
        self.rendered = true;
    }
    fn initialize(
        &mut self,
        _window: void_architect_engine_client::platform::WindowHandle,
    ) -> Result<(), String> {
        self.initialize = true;
        Ok(())
    }
    fn shutdown(&mut self) -> Result<(), String> {
        self.shutdown = true;
        Ok(())
    }
    fn resize(&mut self, _width: u32, _height: u32) -> Result<(), String> {
        self.resize = true;
        Ok(())
    }
}

#[cfg(not(target_os = "macos"))]
#[test]
fn platform_layer_new_initializes_empty() {
    let platform: PlatformLayer<MockEngineApp> = PlatformLayer::new();
    assert!(platform.event_loop.is_none());
    assert!(platform.window.is_none());
    assert!(platform.engine_app.is_none());
}

#[cfg(not(target_os = "macos"))]
#[test]
fn platform_layer_initialize_sets_title_and_event_loop() {
    let mut platform: PlatformLayer<MockEngineApp> = PlatformLayer::new();
    platform.initialize("Test Window");
    assert_eq!(platform.title, "Test Window");
    assert!(platform.event_loop.is_some());
}

#[cfg(not(target_os = "macos"))]
#[test]
#[should_panic(expected = "Event loop must be initialized before run()")]
fn platform_layer_run_panics_if_not_initialized() {
    let mut platform: PlatformLayer<MockEngineApp> = PlatformLayer::new();
    let app = MockEngineApp::default();
    // Should panic because event_loop is None
    platform.run(app);
}

#[cfg(not(target_os = "macos"))]
#[test]
fn platform_system_initializes_event_loop() {
    let mut platform = PlatformSystem::new();
    platform.initialize();
    assert!(
        platform.event_loop.is_some(),
        "Event loop should be initialized"
    );
}

#[cfg(not(target_os = "macos"))]
#[test]
fn engine_client_initializes_platform_system() {
    let mut engine = EngineClient::new();
    engine.initialize();
    assert!(
        engine.platform_system.is_some(),
        "Platform system should be initialized"
    );
    assert!(
        engine.platform_system.as_ref().unwrap().event_loop.is_some(),
        "Event loop should be initialized by EngineClient"
    );
}
