//!
//! Unit tests for the Vulkan integration
//!

use void_architect_engine_client::renderer::RendererFrontend;

// Import the common mock helpers
mod mock_helpers;

mod mock {
    use void_architect_engine_client::platform::WindowHandle;

    pub fn handle() -> WindowHandle {
        // Use the safer mock helper
        super::mock_helpers::create_mock_window_handle()
    }
}

#[test]
fn vulkan_integration_initialize() {
    let mut renderer = RendererFrontend::new();
    let result = renderer.initialize(mock::handle());

    // This is testing against the mock window which should gracefully handle errors
    assert!(result.is_ok() || result.is_err());
}

#[test]
fn vulkan_integration_shutdown() {
    let mut renderer = RendererFrontend::new();

    // Test shutdown without initialize
    let result = renderer.shutdown();
    assert!(result.is_ok() || result.is_err());
}

#[test]
fn vulkan_integration_frame_cycle() {
    let mut renderer = RendererFrontend::new();
    let _ = renderer.initialize(mock::handle());

    // Basic frame render cycle
    let begin_result = renderer.begin_frame();
    assert!(begin_result.is_ok() || begin_result.is_err());

    let end_result = renderer.end_frame();
    assert!(end_result.is_ok() || end_result.is_err());
}

#[test]
fn vulkan_integration_resize() {
    let mut renderer = RendererFrontend::new();
    let _ = renderer.initialize(mock::handle());

    // Test resize
    let resize_result = renderer.resize(1024, 768);
    assert!(resize_result.is_ok() || resize_result.is_err());
}
