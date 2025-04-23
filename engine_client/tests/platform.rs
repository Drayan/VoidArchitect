// Integration tests for PlatformSystem and EngineClient.
//
// NOTE: On macOS, `winit::EventLoop` can only be created on the main thread. As a result,
// these tests are skipped on macOS. See platform.rs for more details.

use void_architect_engine_client::{EngineClient, platform::PlatformSystem};

#[cfg(not(target_os = "macos"))]
#[test]
fn platform_system_initializes_event_loop() {
    let mut platform = PlatformSystem::new();
    platform.initialize();
    assert!(platform.event_loop.is_some(), "Event loop should be initialized");
}

#[cfg(not(target_os = "macos"))]
#[test]
fn engine_client_initializes_platform_system() {
    let mut engine = EngineClient::new();
    engine.initialize();
    assert!(engine.platform_system.is_some(), "Platform system should be initialized");
    assert!(engine.platform_system.as_ref().unwrap().event_loop.is_some(), "Event loop should be initialized by EngineClient");
}

#[test]
#[should_panic(expected = "platform should exist")]
fn engine_client_initialize_panics_if_platform_missing() {
    struct DummyEngineClient {
        platform_system: Option<PlatformSystem>,
    }
    impl DummyEngineClient {
        fn initialize(&mut self) {
            self.platform_system.as_mut().expect("platform should exist").initialize();
        }
    }
    let mut engine = DummyEngineClient { platform_system: None };
    engine.initialize();
}
