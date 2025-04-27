//!
//! Unit tests for error handling in the engine client
//!

use void_architect_engine_client::platform::WindowHandle;
use void_architect_engine_client::renderer::{RendererBackend, RendererFrontend};

// Import the common mock helpers
mod mock_helpers;

// Define a mock renderer backend for testing error handling
struct MockErrorRendererBackend;

impl RendererBackend for MockErrorRendererBackend {
    fn initialize(&mut self, _window: WindowHandle) -> Result<(), String> {
        // Simulate an initialization error
        Err("Mock initialization error".to_string())
    }

    fn shutdown(&mut self) -> Result<(), String> {
        Ok(()) // Mock shutdown succeeds
    }

    fn begin_frame(&mut self) -> Result<(), String> {
        Err("Mock begin frame error".to_string()) // Simulate an error
    }

    fn end_frame(&mut self) -> Result<(), String> {
        Err("Mock end frame error".to_string()) // Simulate an error
    }

    fn resize(&mut self, _width: u32, _height: u32) -> Result<(), String> {
        Err("Mock resize error".to_string()) // Simulate an error
    }
}

#[test]
fn engine_handles_initialization_errors() {
    // Create a RendererFrontend with the mock error backend
    let mut renderer = RendererFrontend {
        backend: Box::new(MockErrorRendererBackend),
    };

    // Try to initialize (should fail)
    let result = renderer.initialize(mock_helpers::create_mock_window_handle());

    // Check that we get the appropriate error message from the mock
    assert!(
        result.is_err(),
        "Renderer initialization with mock error backend should fail"
    );
    if let Err(err) = result {
        assert!(
            err.contains("Mock initialization error"),
            "Error message should indicate mock initialization failure: {}",
            err
        );
    }
}

#[test]
fn engine_handles_resize_errors() {
    let mut renderer = RendererFrontend {
        backend: Box::new(MockErrorRendererBackend),
    };

    // Try to resize before initialize (should fail)
    let result = renderer.resize(0, 0);

    // Should fail with an appropriate error from the mock
    assert!(
        result.is_err(),
        "Resize with invalid dimensions should fail"
    );
    if let Err(err) = result {
        assert!(
            err.contains("Mock resize error"),
            "Error message should indicate mock resize failure: {}",
            err
        );
    }
}

#[test]
fn engine_handles_frame_errors() {
    let mut renderer = RendererFrontend {
        backend: Box::new(MockErrorRendererBackend),
    };

    // Begin frame without initialize should fail with an error from the mock
    let result = renderer.begin_frame();
    assert!(
        result.is_err(),
        "Begin frame without initialize should fail"
    );
    if let Err(err) = result {
        assert!(
            err.contains("Mock begin frame error"),
            "Error message should indicate mock begin frame failure: {}",
            err
        );
    }

    // End frame without initialize should fail with an error from the mock
    let result = renderer.end_frame();
    assert!(result.is_err(), "End frame without initialize should fail");
    if let Err(err) = result {
        assert!(
            err.contains("Mock end frame error"),
            "Error message should indicate mock end frame failure: {}",
            err
        );
    }
}
